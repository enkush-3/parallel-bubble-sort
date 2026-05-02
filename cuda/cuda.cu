// cuda.cu
// nvcc -O2 -o cuda cuda.cu

#include <iostream>
#include <vector>
#include <cstdlib>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <cuda_runtime.h>

using namespace std;

#define CUDA_CHECK(call) \
    do { cudaError_t e = (call); if (e != cudaSuccess) { \
        fprintf(stderr, "CUDA error %s:%d: %s\n", __FILE__, __LINE__, cudaGetErrorString(e)); exit(1); } \
    } while(0)

struct Metrics {
    int N;
    double cpu_time, gpu_kernel, h2d, d2h, transfer, exec;
    double data_mb, total_ops, cpu_gops, gpu_gops, speedup, eff_speedup;
    bool correct;
};

void bubbleSortCPU(vector<float>& a, int n) {
    for (int i = 0; i < n-1; i++)
        for (int j = 0; j < n-1-i; j++)
            if (a[j] > a[j+1]) swap(a[j], a[j+1]);
}

__global__ void oddEvenKernel(float* d_a, int n, int phase) {
    int idx = (blockIdx.x*blockDim.x + threadIdx.x)*2 + phase;
    if (idx < n-1 && d_a[idx] > d_a[idx+1]) {
        float t = d_a[idx]; d_a[idx] = d_a[idx+1]; d_a[idx+1] = t;
    }
}

bool verify(const vector<float>& cpu, const vector<float>& gpu, int n) {
    for (int i = 0; i < n; i++) if (fabs(cpu[i]-gpu[i]) > 1e-5) return false;
    return true;
}

Metrics runExperiment(int N) {
    size_t sz = N * sizeof(float);
    Metrics m;
    m.N = N;
    m.data_mb = 2.0 * sz / (1024*1024);
    vector<float> h_cpu(N), h_gpu(N), h_res(N);
    srand(42);
    for (int i = 0; i < N; i++) h_cpu[i] = h_gpu[i] = rand() % 1000000;

    auto t0 = chrono::high_resolution_clock::now();
    bubbleSortCPU(h_cpu, N);
    auto t1 = chrono::high_resolution_clock::now();
    m.cpu_time = chrono::duration<double>(t1-t0).count();

    float *d_a;
    CUDA_CHECK(cudaMalloc(&d_a, sz));
    cudaEvent_t start, stop;
    CUDA_CHECK(cudaEventCreate(&start)); CUDA_CHECK(cudaEventCreate(&stop));

    // H2D
    CUDA_CHECK(cudaEventRecord(start));
    CUDA_CHECK(cudaMemcpy(d_a, h_gpu.data(), sz, cudaMemcpyHostToDevice));
    CUDA_CHECK(cudaEventRecord(stop)); CUDA_CHECK(cudaEventSynchronize(stop));
    float ms; CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
    m.h2d = ms/1000.0;

    // Kernel
    int threads = 256, blocks = ((N/2)+threads-1)/threads;
    CUDA_CHECK(cudaEventRecord(start));
    for (int i = 0; i < N; i++) {
        oddEvenKernel<<<blocks, threads>>>(d_a, N, i%2);
        CUDA_CHECK(cudaDeviceSynchronize());
    }
    CUDA_CHECK(cudaEventRecord(stop)); CUDA_CHECK(cudaEventSynchronize(stop));
    CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
    m.gpu_kernel = ms/1000.0;

    // D2H
    CUDA_CHECK(cudaEventRecord(start));
    CUDA_CHECK(cudaMemcpy(h_res.data(), d_a, sz, cudaMemcpyDeviceToHost));
    CUDA_CHECK(cudaEventRecord(stop)); CUDA_CHECK(cudaEventSynchronize(stop));
    CUDA_CHECK(cudaEventElapsedTime(&ms, start, stop));
    m.d2h = ms/1000.0;

    m.transfer = m.h2d + m.d2h;
    m.exec = m.gpu_kernel + m.transfer;
    m.total_ops = 1.0 * N * N;
    m.cpu_gops = m.total_ops / m.cpu_time / 1e9;
    m.gpu_gops = m.total_ops / m.gpu_kernel / 1e9;
    m.speedup = m.cpu_time / m.gpu_kernel;
    m.eff_speedup = m.cpu_time / m.exec;
    m.correct = verify(h_cpu, h_res, N);

    CUDA_CHECK(cudaFree(d_a));
    CUDA_CHECK(cudaEventDestroy(start)); CUDA_CHECK(cudaEventDestroy(stop));
    return m;
}

void printDetailed(const Metrics& m, const cudaDeviceProp& prop) {
    const int col1 = 28;   // Метрийн нэр
    const int col2 = 22;   // CPU утга
    const int col3 = 22;   // GPU утга
    string sep(col1 + col2 + col3, '=');
    string line(col1 + col2 + col3, '-');
    
    cout << "\n" << sep << "\n";
    cout << "  N = " << m.N << " | GPU: " << prop.name << " | Result: " << (m.correct ? "CORRECT" : "FAILED") << "\n";
    cout << sep << "\n";
    cout << left << setw(col1) << "МЕТРИК (Үнэлгээ)"
         << setw(col2) << "CPU"
         << setw(col3) << "GPU" << "\n";
    cout << line << "\n";
    
    auto row = [&](const string& name, const string& cpu_val, const string& gpu_val) {
        cout << left << setw(col1) << name
             << setw(col2) << cpu_val
             << setw(col3) << gpu_val << "\n";
    };
    
    auto d2str = [](double v, const string& unit) -> string {
        ostringstream os;
        if (v < 0.001) os << fixed << setprecision(3) << v*1000.0 << " " << unit;
        else if (v < 1.0) os << fixed << setprecision(3) << v*1000.0 << " " << unit;
        else os << fixed << setprecision(2) << v << " " << unit;
        return os.str();
    };
    
    // CPU Time
    row("CPU Time", d2str(m.cpu_time, "sec"), "-");
    // GPU Kernel Time
    row("GPU Kernel Time", "-", d2str(m.gpu_kernel, "sec"));
    // Host→Device Transfer
    row("Host→Device Transfer  ", "-", d2str(m.h2d, "sec"));
    // Device→Host Transfer
    row("Device→Host Transfer  ", "-", d2str(m.d2h, "sec"));
    // Total Transfer
    row("Total Transfer", "-", d2str(m.transfer, "sec"));
    // GPU Total (kernel+transfer)
    row("GPU Total (kernel+trans)", "-", d2str(m.exec, "sec"));
    cout << line << "\n";
    
    // Data transferred
    row("Data transferred", "-", d2str(m.data_mb, "MB"));
    // Total operations
    string ops_str;
    if (m.total_ops > 1e9) ops_str = to_string((long long)(m.total_ops/1e9)) + "e9 ops";
    else if (m.total_ops > 1e6) ops_str = to_string((long long)(m.total_ops/1e6)) + "e6 ops";
    else ops_str = to_string((long long)m.total_ops) + " ops";
    row("Total operations", ops_str, ops_str);
    
    // Performance
    row("Performance (GOPS/s)", d2str(m.cpu_gops, "GOPS/s"), d2str(m.gpu_gops, "GOPS/s"));
    
    cout << sep << "\n";
    cout << "  ХАРЬЦУУЛАЛТ / ДҮГНЭЛТ:\n";
    cout << "  SpeedUp (CPU/GPU kernel)   : " << fixed << setprecision(2) << m.speedup << "x\n";
    cout << "  Eff.SpeedUp (incl.transfer): " << fixed << setprecision(2) << m.eff_speedup << "x\n";
    cout << sep << "\n";
}

void saveCSV(const vector<Metrics>& results, const string& fname) {
    FILE* f = fopen(fname.c_str(), "w");
    if (!f) return;
    fprintf(f, "N,cpu_time,gpu_kernel,h2d,d2h,transfer,total_exec,data_mb,total_ops,cpu_gops,gpu_gops,speedup,eff_speedup,correct\n");
    for (auto& m : results)
        fprintf(f, "%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.3f,%.3e,%.4f,%.4f,%.2f,%.2f,%d\n",
                m.N, m.cpu_time, m.gpu_kernel, m.h2d, m.d2h, m.transfer, m.exec,
                m.data_mb, m.total_ops, m.cpu_gops, m.gpu_gops, m.speedup, m.eff_speedup, (int)m.correct);
    fclose(f);
    cout << "\n[CSV] Saved: " << fname << "\n";
}

int main() {
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    cout << "GPU: " << prop.name << " | SMs: " << prop.multiProcessorCount
         << " | Global: " << prop.totalGlobalMem/(1024*1024) << " MB\n";

    vector<int> sizes = {5000, 10000, 50000, 100000};
    vector<Metrics> results;
    for (int N : sizes) {
        cout << "\n>>> Running N = " << N << " ..." << flush;
        Metrics m = runExperiment(N);
        cout << " done." << endl;
        printDetailed(m, prop);
        results.push_back(m);
    }
    saveCSV(results, "results.csv");
    return 0;
}

/*

nvcc -O2 cuda.cu -o cuda
./cuda
python plot.py

*/