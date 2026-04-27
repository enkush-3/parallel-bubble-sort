#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdlib>

using namespace std;

// =====================================================
// SEQUENTIAL BUBBLE SORT
// =====================================================
void bubbleSequential(vector<float>& a, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n - 1; j++) {
            if (a[j] > a[j + 1]) {
                swap(a[j], a[j + 1]);
            }
        }
    }
}

// =====================================================
// THREAD WORK FUNCTION (handles a chunk of comparisons)
// =====================================================
void compareSwap(vector<float>& a, int start, int end, int phase, int n) {

    // Each thread works on independent pairs
    for (int i = start; i < end; i++) {

        int idx = phase + 2 * i;

        if (idx < n - 1) {
            if (a[idx] > a[idx + 1]) {
                swap(a[idx], a[idx + 1]);
            }
        }
    }
}

// =====================================================
// PARALLEL ODD-EVEN SORT USING std::thread
// =====================================================
void bubbleParallel(vector<float>& a, int n, int num_threads) {

    for (int phase = 0; phase < n; phase++) {

        vector<thread> threads;

        int phaseType = phase % 2; // even or odd phase

        int totalPairs = (n - phaseType) / 2;

        int chunkSize = (totalPairs + num_threads - 1) / num_threads;

        // Create threads
        for (int t = 0; t < num_threads; t++) {

            int start = t * chunkSize;
            int end = min(start + chunkSize, totalPairs);

            threads.push_back(thread(compareSwap,
                                     ref(a),
                                     start,
                                     end,
                                     phaseType,
                                     n));
        }

        // Synchronization: wait for all threads
        for (auto& th : threads) {
            th.join();
        }
    }
}

// =====================================================
// MAIN
// =====================================================
int main() {

    int N = 100000; // test sizes: 10k, 100k, 1M
    int num_threads = thread::hardware_concurrency();

    vector<float> a(N), b(N);

    // Random data
    for (int i = 0; i < N; i++) {
        a[i] = rand() % 100000;
        b[i] = a[i];
    }

    // ================= CPU SEQUENTIAL =================
    auto start_cpu = chrono::high_resolution_clock::now();

    bubbleSequential(a, N);

    auto end_cpu = chrono::high_resolution_clock::now();

    double cpu_time =
        chrono::duration<double>(end_cpu - start_cpu).count();

    // ================= THREAD VERSION =================
    auto start_par = chrono::high_resolution_clock::now();

    bubbleParallel(b, N, num_threads);

    auto end_par = chrono::high_resolution_clock::now();

    double par_time =
        chrono::duration<double>(end_par - start_par).count();

    // ================= METRICS =================
    double speedup = cpu_time / par_time;

    double total_ops = (double)N * (double)N;

    double cpu_perf = total_ops / cpu_time;
    double par_perf = total_ops / par_time;

    // ================= OUTPUT =================
    cout << "\n========= RESULTS =========\n";

    cout << "Array Size: " << N << "\n";

    cout << "CPU Time: " << cpu_time << " sec\n";
    cout << "Thread Time: " << par_time << " sec\n";

    cout << "Threads Used: " << num_threads << "\n";

    cout << "SpeedUp: " << speedup << "x\n";

    cout << "Total Operations: " << total_ops << "\n";

    cout << "CPU Performance: " << cpu_perf << " ops/sec\n";
    cout << "Thread Performance: " << par_perf << " ops/sec\n";

    cout << "===========================\n";

    return 0;
}