#include "../include/algorithm.h"

#include <algorithm>
#include <omp.h>

namespace {

bool runOpenMPPhase(std::vector<int>& arr, std::size_t phase) {
    const std::size_t startIndex = phase & 1U;
    const std::size_t pairCount = (arr.size() - startIndex) / 2;
    int swapped = 0;

#pragma omp parallel for schedule(static) reduction(| : swapped)
    for (long long pairIndex = 0; pairIndex < static_cast<long long>(pairCount); ++pairIndex) {
        const std::size_t left = startIndex + static_cast<std::size_t>(pairIndex) * 2;
        if (arr[left] > arr[left + 1]) {
            std::swap(arr[left], arr[left + 1]);
            swapped = 1;
        }
    }

    return swapped != 0;
}

} // namespace

void sortOpenMP(std::vector<int>& arr) {
    const std::size_t n = arr.size();

    if (n < 2) {
        return;
    }

    for (std::size_t phase = 0; phase < n; phase += 2) {
        bool cycleSwapped = runOpenMPPhase(arr, phase);

        if (phase + 1 < n) {
            cycleSwapped = runOpenMPPhase(arr, phase + 1) || cycleSwapped;
        }

        if (!cycleSwapped) {
            break;
        }
    }
}
