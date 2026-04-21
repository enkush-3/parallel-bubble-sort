#include <iostream>
#include <vector>
#include <cstdlib>
#include <chrono>
#include <cuda_runtime.h>

using namespace std;

// =====================================================
// CPU SEQUENTIAL BUBBLE SORT
// =====================================================
void bubbleSeq(vector<float>& a, int n){
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n - 1; j++){
            if(a[j] > a[j + 1]){
                swap(a[j], a[j + 1]);
            }
        }
    }
}

// =====================================================
// CUDA KERNEL (ODD-EVEN SORT)
// =====================================================
__global__ void oddEvenKernel(float* d_a, int n, int phase){
    int idx = (blockIdx.x * blockDim.x + threadIdx.x) * 2 + phase;

    if(idx < n - 1){
        if(d_a[idx] > d_a[idx + 1]){
            float temp = d_a[idx];
            d_a[idx] = d_a[idx + 1];
            d_a[idx + 1] = temp;
        }
    }
}

// =====================================================
// MAIN
// =====================================================
int main(){

    int N = 50000;
    size_t size = N * sizeof(float);

    vector<float> h_cpu(N), h_gpu(N);

    for(int i = 0; i < N; i++){
        h_cpu[i] = rand() % 100000;
        h_gpu[i] = h_cpu[i];
    }

    // =====================================================
    // CPU TIMER
    // =====================================================
    auto start_cpu = chrono::high_resolution_clock::now();

    bubbleSeq(h_cpu, N);

    auto end_cpu = chrono::high_resolution_clock::now();

    double cpu_time =
        chrono::duration<double>(end_cpu - start_cpu).count();

    // =====================================================
    // GPU SETUP
    // =====================================================
    float *d_a;
    cudaMalloc(&d_a, size);
    cudaMemcpy(d_a, h_gpu.data(), size, cudaMemcpyHostToDevice);

    int threads = 256;
    int blocks = (N/2 + threads - 1) / threads;

    // =====================================================
    // GPU TIMER
    // =====================================================
    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    cudaEventRecord(start);

    for(int i = 0; i < N; i++){
        oddEvenKernel<<<blocks, threads>>>(d_a, N, i % 2);
        cudaDeviceSynchronize();
    }

    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float gpu_time = 0;
    cudaEventElapsedTime(&gpu_time, start, stop);

    cudaMemcpy(h_gpu.data(), d_a, size, cudaMemcpyDeviceToHost);

    // =====================================================
    // DATA TRANSFER TIME
    // =====================================================
    auto transfer_start = chrono::high_resolution_clock::now();

    cudaMemcpy(d_a, h_gpu.data(), size, cudaMemcpyHostToDevice);
    cudaMemcpy(h_gpu.data(), d_a, size, cudaMemcpyDeviceToHost);

    auto transfer_end = chrono::high_resolution_clock::now();

    double transfer_time =
        chrono::duration<double>(transfer_end - transfer_start).count();

    // =====================================================
    // METRICS
    // =====================================================

    double gpu_time_sec = gpu_time / 1000.0;

    double speedup = cpu_time / gpu_time_sec;

    double total_ops = (double)N * (double)N; // O(n^2)

    double cpu_perf = total_ops / cpu_time;
    double gpu_perf = total_ops / gpu_time_sec;

    // =====================================================
    // OUTPUT REPORT
    // =====================================================

    cout << "\n========= RESULTS =========\n";

    cout << "CPU Time: " << cpu_time << " sec\n";
    cout << "GPU Time: " << gpu_time_sec << " sec\n";
    cout << "Data Transfer Time: " << transfer_time << " sec\n";

    cout << "Total Data Transferred: "
         << (2.0 * size / (1024 * 1024)) << " MB\n";

    cout << "SpeedUp (CPU/GPU): " << speedup << "x\n";

    cout << "Total Operations (approx): " << total_ops << "\n";

    cout << "CPU Performance: " << cpu_perf << " ops/sec\n";
    cout << "GPU Performance: " << gpu_perf << " ops/sec\n";

    cout << "Execution Time (GPU + transfer): "
         << gpu_time_sec + transfer_time << " sec\n";

    cout << "===========================\n";

    cudaFree(d_a);

    return 0;
}