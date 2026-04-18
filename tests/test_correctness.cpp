#include "../include/algorithm.h"

#include <algorithm>
#include <exception>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using SortFunction = void (*)(std::vector<int>&);

std::vector<int> makeRandomData(std::size_t size, unsigned int seed) {
	std::vector<int> data(size);
	std::mt19937 generator(seed);
	std::uniform_int_distribution<int> distribution(-1000, 1000);

	for (int& value : data) {
		value = distribution(generator);
	}

	return data;
}

void verifyCase(const std::string& name, SortFunction sorter, const std::vector<int>& input) {
	std::vector<int> expected = input;
	std::sort(expected.begin(), expected.end());

	std::vector<int> actual = input;
	sorter(actual);

	if (actual != expected) {
		throw std::runtime_error(name + " failed");
	}
}

void verifyAlgorithm(const std::string& name, SortFunction sorter) {
	verifyCase(name + " empty", sorter, {});
	verifyCase(name + " single", sorter, {42});
	verifyCase(name + " sorted", sorter, {1, 2, 3, 4, 5});
	verifyCase(name + " reverse", sorter, {5, 4, 3, 2, 1});
	verifyCase(name + " duplicates", sorter, {3, 1, 2, 3, 1, 0, -4, 3});
	verifyCase(name + " random", sorter, makeRandomData(64, 1337));
}

} // namespace

int main() {
	try {
		verifyAlgorithm("Sequential", sortSequential);
		verifyAlgorithm("StdThread", sortStdThread);
		verifyAlgorithm("OpenMP", sortOpenMP);
		verifyAlgorithm("CUDA", sortCUDA);

		std::cout << "All correctness checks passed.\n";
		return 0;
	} catch (const std::exception& exception) {
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
