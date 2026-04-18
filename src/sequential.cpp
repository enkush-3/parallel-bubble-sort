#include "../include/algorithm.h"

#include <algorithm>

void sortSequential(std::vector<int>& arr) {
    const std::size_t n = arr.size();

    if (n < 2) {
        return;
    }

    for (std::size_t pass = 0; pass + 1 < n; ++pass) {
        bool swapped = false;

        for (std::size_t index = 0; index + 1 < n - pass; ++index) {
            if (arr[index] > arr[index + 1]) {
                std::swap(arr[index], arr[index + 1]);
                swapped = true;
            }
        }

        if (!swapped) {
            break;
        }
    }
}
