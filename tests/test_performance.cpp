#include "../include/algorithm.h"
#include "../include/timer.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<int> makeRandomData(std::size_t size) {
	std::vector<int> data(size);
	std::mt19937 generator(2026);
	std::uniform_int_distribution<int> distribution(1, 1'000'000);

	for (int& value : data) {
		value = distribution(generator);
	}

	return data;
}

template <typename SortFunction>
double measure(const std::string& name, SortFunction sorter, const std::vector<int>& input) {
	std::vector<int> data = input;
	Timer timer;

	timer.start();
	sorter(data);
	const double elapsedMs = timer.elapsed();

	if (!std::is_sorted(data.begin(), data.end())) {
		throw std::runtime_error(name + " failed to sort the input");
	}

	std::cout << std::left << std::setw(16) << name << ": " << std::fixed << std::setprecision(3)
			  << elapsedMs << " ms\n";
	return elapsedMs;
}

void runBenchmark(std::size_t size) {
	std::cout << "\n=== Performance sample: " << size << " elements ===\n";
	const std::vector<int> input = makeRandomData(size);

	measure("Sequential", sortSequential, input);
	measure("StdThread", sortStdThread, input);
	measure("OpenMP", sortOpenMP, input);
	measure("CUDA", sortCUDA, input);
}

} // namespace

int main() {
	try {
		const std::array<std::size_t, 3> sampleSizes = {512, 1024, 2048};

		for (std::size_t size : sampleSizes) {
			runBenchmark(size);
		}

		return 0;
	} catch (const std::exception& exception) {
		std::cerr << "Performance test failed: " << exception.what() << '\n';
		return 1;
	}
}
