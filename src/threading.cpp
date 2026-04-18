#include "../include/algorithm.h"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

namespace {

class Barrier {
public:
    explicit Barrier(std::size_t participantCount)
        : participantCount_(participantCount), waitingCount_(participantCount) {}

    void arriveAndWait() {
        std::unique_lock<std::mutex> lock(mutex_);
        const std::size_t generation = generation_;

        if (--waitingCount_ == 0) {
            ++generation_;
            waitingCount_ = participantCount_;
            condition_.notify_all();
            return;
        }

        condition_.wait(lock, [this, generation] { return generation_ != generation; });
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t participantCount_;
    std::size_t waitingCount_;
    std::size_t generation_ = 0;
};

} // namespace

void sortStdThread(std::vector<int>& arr) {
    const std::size_t n = arr.size();

    if (n < 2) {
        return;
    }

    std::size_t hardwareThreads = std::thread::hardware_concurrency();
    if (hardwareThreads == 0) {
        hardwareThreads = 2;
    }

    const std::size_t maxUsefulThreads = std::max<std::size_t>(1, n / 2);
    const std::size_t threadCount = std::min(hardwareThreads, maxUsefulThreads);

    if (threadCount < 2) {
        sortSequential(arr);
        return;
    }

    std::atomic<bool> stop{false};
    std::atomic<bool> swapped{false};
    std::atomic<std::size_t> currentPhase{0};
    Barrier barrier(threadCount + 1);

    std::vector<std::thread> workers;
    workers.reserve(threadCount);

    for (std::size_t workerId = 0; workerId < threadCount; ++workerId) {
        workers.emplace_back([&, workerId] {
            while (true) {
                barrier.arriveAndWait();

                if (stop.load(std::memory_order_acquire)) {
                    break;
                }

                const std::size_t phase = currentPhase.load(std::memory_order_relaxed);
                const std::size_t startIndex = phase & 1U;
                const std::size_t pairCount = (n - startIndex) / 2;

                for (std::size_t pairIndex = workerId; pairIndex < pairCount; pairIndex += threadCount) {
                    const std::size_t left = startIndex + pairIndex * 2;
                    if (arr[left] > arr[left + 1]) {
                        std::swap(arr[left], arr[left + 1]);
                        swapped.store(true, std::memory_order_relaxed);
                    }
                }

                barrier.arriveAndWait();
            }
        });
    }

    for (std::size_t phase = 0; phase < n; phase += 2) {
        bool cycleSwapped = false;

        currentPhase.store(phase, std::memory_order_relaxed);
        swapped.store(false, std::memory_order_relaxed);
        barrier.arriveAndWait();
        barrier.arriveAndWait();
        cycleSwapped |= swapped.load(std::memory_order_relaxed);

        if (phase + 1 < n) {
            currentPhase.store(phase + 1, std::memory_order_relaxed);
            swapped.store(false, std::memory_order_relaxed);
            barrier.arriveAndWait();
            barrier.arriveAndWait();
            cycleSwapped |= swapped.load(std::memory_order_relaxed);
        }

        if (!cycleSwapped) {
            break;
        }
    }

    stop.store(true, std::memory_order_release);
    barrier.arriveAndWait();

    for (std::thread& worker : workers) {
        worker.join();
    }
}
