// sort_compare.cu
// nvcc -O2 -o sort_compare sort_compare.cu

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

void printDetailed(const Metrics& m) {
    cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    cout << "║   N = " << setw(7) << m.N << "  |  Result: " << (m.correct ? "✓ CORRECT" : "✗ FAILED") << "                     ║\n";
    cout << "╠══════════════════════════════════════════════════════════╣\n";
    cout << "║  CPU Time                 " << setw(14) << fixed << setprecision(6) << m.cpu_time << "  sec     ║\n";
    cout << "║  GPU Kernel Time          " << setw(14) << m.gpu_kernel << "  sec     ║\n";
    cout << "║  Host→Device Transfer     " << setw(14) << m.h2d << "  sec     ║\n";
    cout << "║  Device→Host Transfer     " << setw(14) << m.d2h << "  sec     ║\n";
    cout << "║  Total Transfer           " << setw(14) << m.transfer << "  sec     ║\n";
    cout << "║  GPU Total (kernel+trans) " << setw(14) << m.exec << "  sec     ║\n";
    cout << "╠══════════════════════════════════════════════════════════╣\n";
    cout << "║  Data transferred         " << setw(14) << setprecision(3) << m.data_mb << "  MB       ║\n";
    cout << "║  Total operations (approx)" << setw(14) << scientific << setprecision(3) << m.total_ops << "  ops      ║\n";
    cout << "╠══════════════════════════════════════════════════════════╣\n";
    cout << "║  CPU Performance          " << setw(14) << fixed << setprecision(4) << m.cpu_gops << "  GOps/s   ║\n";
    cout << "║  GPU Performance          " << setw(14) << m.gpu_gops << "  GOps/s   ║\n";
    cout << "╠══════════════════════════════════════════════════════════╣\n";
    cout << "║  SpeedUp (CPU/GPU kernel) " << setw(14) << setprecision(2) << m.speedup << "  x        ║\n";
    cout << "║  Eff.SpeedUp (incl.trans) " << setw(14) << m.eff_speedup << "  x        ║\n";
    cout << "╚══════════════════════════════════════════════════════════╝\n";
}

void printSummary(const vector<Metrics>& results) {
    cout << "\n\n================================= SUMMARY =================================\n";
    cout << setw(8) << "N" << setw(12) << "CPU(s)" << setw(12) << "GPU(s)" << setw(12) << "Trans(s)"
         << setw(12) << "GPU tot" << setw(9) << "SpdUp" << setw(9) << "EffSp" << "\n";
    for (auto& m : results) {
        cout << setw(8) << m.N << fixed << setprecision(4)
             << setw(12) << m.cpu_time << setw(12) << m.gpu_kernel << setw(12) << m.transfer
             << setw(12) << m.exec << setw(9) << setprecision(2) << m.speedup
             << setw(9) << m.eff_speedup << "\n";
    }
    cout << "=============================================================================\n";
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

    vector<int> sizes = {10000, 50000, 100000};
    vector<Metrics> results;
    for (int N : sizes) {
        cout << "\n>>> Running N = " << N << " ..." << flush;
        Metrics m = runExperiment(N);
        cout << " done." << endl;
        printDetailed(m);   // <-- ЭНД БҮРЭН ХҮСНЭГТ ХЭВЛЭНЭ
        results.push_back(m);
    }
    printSummary(results);
    saveCSV(results, "results.csv");
    return 0;
}