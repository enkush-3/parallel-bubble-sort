#include <iostream>
#include <vector>
#include <random>
#include "../include/algorithm.h"
#include "../include/timer.h"

void generateData(std::vector<int>& arr, int size) {
    arr.resize(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 1000000);
    for (int i = 0; i < size; ++i) {
        arr[i] = dis(gen);
    }
}

void runBenchmark(int size) {
    std::cout << "\n=== ТЕСТ: " << size << " элемент ===" << std::endl;
    std::vector<int> original_data;
    generateData(original_data, size);
    Timer timer;

    // 1. Sequential
    std::vector<int> seq_data = original_data;
    timer.start();
    sortSequential(seq_data);
    std::cout << "Sequential Time: " << timer.elapsed() << " ms\n";

    // 2. OpenMP
    std::vector<int> omp_data = original_data;
    timer.start();
    sortOpenMP(omp_data);
    std::cout << "OpenMP Time: " << timer.elapsed() << " ms\n";

    // 3. std::thread ба CUDA-г мөн ижил аргаар дуудаж хэмжинэ.
}

int main() {
    std::vector<int> sizes = {10000, 100000, 1000000}; // 10k, 100k, 1M 
    
    for (int size : sizes) {
        runBenchmark(size);
    }
    
    return 0;
}