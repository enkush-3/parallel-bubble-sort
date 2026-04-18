#include "../include/algorithm.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

void checkCuda(cudaError_t error, const char* operation) {
    if (error != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(error));
    }
}

struct DeviceArray {
    int* ptr = nullptr;

    ~DeviceArray() {
        if (ptr != nullptr) {
            cudaFree(ptr);
        }
    }
};

__global__ void bubblePhaseKernel(int* values, int n, int phase, int* swappedFlag) {
    const int pairIndex = blockIdx.x * blockDim.x + threadIdx.x;
    const int startIndex = phase & 1;
    const int left = startIndex + pairIndex * 2;

    if (left + 1 < n) {
        const int firstValue = values[left];
        const int secondValue = values[left + 1];

        if (firstValue > secondValue) {
            values[left] = secondValue;
            values[left + 1] = firstValue;
            atomicExch(swappedFlag, 1);
        }
    }
}

bool runCudaPhase(int* values, int n, int phase, int* swappedFlag) {
    const int startIndex = phase & 1;
    const int pairCount = (n - startIndex) / 2;

    if (pairCount <= 0) {
        return false;
    }

    checkCuda(cudaMemset(swappedFlag, 0, sizeof(int)), "cudaMemset");

    const int blockSize = 256;
    const int gridSize = (pairCount + blockSize - 1) / blockSize;

    bubblePhaseKernel<<<gridSize, blockSize>>>(values, n, phase, swappedFlag);
    checkCuda(cudaGetLastError(), "bubblePhaseKernel launch");
    checkCuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");

    int hostSwapped = 0;
    checkCuda(cudaMemcpy(&hostSwapped, swappedFlag, sizeof(int), cudaMemcpyDeviceToHost), "cudaMemcpy");
    return hostSwapped != 0;
}

} // namespace

void sortCUDA(std::vector<int>& arr) {
    const std::size_t n = arr.size();

    if (n < 2) {
        return;
    }

    int deviceCount = 0;
    if (cudaGetDeviceCount(&deviceCount) != cudaSuccess || deviceCount <= 0) {
        sortSequential(arr);
        return;
    }

    if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("CUDA bubble sort only supports arrays that fit into a 32-bit index");
    }

    const int elementCount = static_cast<int>(n);
    const std::size_t byteCount = n * sizeof(int);

    DeviceArray values;
    DeviceArray swappedFlag;

    checkCuda(cudaMalloc(&values.ptr, byteCount), "cudaMalloc(values)");
    checkCuda(cudaMalloc(&swappedFlag.ptr, sizeof(int)), "cudaMalloc(swappedFlag)");
    checkCuda(cudaMemcpy(values.ptr, arr.data(), byteCount, cudaMemcpyHostToDevice), "cudaMemcpy host to device");

    for (int phase = 0; phase < elementCount; phase += 2) {
        bool cycleSwapped = runCudaPhase(values.ptr, elementCount, phase, swappedFlag.ptr);

        if (phase + 1 < elementCount) {
            cycleSwapped = runCudaPhase(values.ptr, elementCount, phase + 1, swappedFlag.ptr) || cycleSwapped;
        }

        if (!cycleSwapped) {
            break;
        }
    }

    checkCuda(cudaMemcpy(arr.data(), values.ptr, byteCount, cudaMemcpyDeviceToHost), "cudaMemcpy device to host");
}
