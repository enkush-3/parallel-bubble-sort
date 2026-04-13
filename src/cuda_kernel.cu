#include "../include/algorithm.h"
#include <cuda_runtime.h>
#include <iostream>

// CUDA Kernel
__global__ void sortKernel(int* d_arr, int n) {
    int idx = threadIdx.x + blockIdx.x * blockDim.x;
    if (idx < n) {
        // Энд алгоритмын логикийг оруулна
    }
}

void sortCUDA(std::vector<int>& arr) {
    int n = arr.size();
    int size = n * sizeof(int);
    int* d_arr;

    // GPU санах ой нөөцлөх
    cudaMalloc((void**)&d_arr, size);

    // Өгөгдлийг Host-оос Device рүү хуулах (Data transfer)
    cudaMemcpy(d_arr, arr.data(), size, cudaMemcpyHostToDevice);

    // Kernel дуудах
    int blockSize = 256;
    int gridSize = (n + blockSize - 1) / blockSize;
    sortKernel<<<gridSize, blockSize>>>(d_arr, n);

    // Үр дүнг Device-аас Host рүү хуулах
    cudaMemcpy(arr.data(), d_arr, size, cudaMemcpyDeviceToHost);

    // Санах ойг чөлөөлөх
    cudaFree(d_arr);
}
