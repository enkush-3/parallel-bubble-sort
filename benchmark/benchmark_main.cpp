#include "../include/algorithm.h"
#include "../include/timer.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<int> generateData(std::size_t size) {
    std::vector<int> data(size);
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<int> distribution(1, 1'000'000);

    for (int& value : data) {
        value = distribution(generator);
    }

    return data;
}

template <typename SortFunction>
double benchmarkVariant(const std::string& name, SortFunction sortFunction, const std::vector<int>& input) {
    std::vector<int> data = input;
    Timer timer;

    timer.start();
    sortFunction(data);
    const double elapsedMs = timer.elapsed();

    if (!std::is_sorted(data.begin(), data.end())) {
        throw std::runtime_error(name + " did not sort the input");
    }

    std::cout << std::left << std::setw(16) << name << ": " << std::fixed << std::setprecision(3)
              << elapsedMs << " ms\n";
    return elapsedMs;
}

void runBenchmark(std::size_t size) {
    std::cout << "\n=== Input size: " << size << " ===\n";

    const std::vector<int> originalData = generateData(size);
    benchmarkVariant("Sequential", sortSequential, originalData);
    benchmarkVariant("StdThread", sortStdThread, originalData);
    benchmarkVariant("OpenMP", sortOpenMP, originalData);
    benchmarkVariant("CUDA", sortCUDA, originalData);
}

}

int main() {
    try {
        const std::vector<std::size_t> sizes = {10000, 100000, 240000};

        for (std::size_t size : sizes) {
            runBenchmark(size);
        }

        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "Benchmark failed: " << exception.what() << '\n';
        return 1;
    }
}