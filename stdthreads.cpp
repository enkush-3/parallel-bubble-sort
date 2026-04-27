/**
 * ============================================================
 * F.CSM306 - Параллел ба Хуваарилагдсан тооцоолол
 * Бие даалт: Bubble Sort Алгоритм: — std::thread хувилбар
 * ============================================================
 *
 * Алгоритм: Parallel Bubble Sort => Odd-Even Transposition Sort
 *           + Thread Pool (condition_variable barrier)
 *
 * Яагаад thread pool хэрэгтэй вэ?
 *   phase бүрт thread үүсгэж, join() хийнэ.
 *   N=100,000 үед 100,000 удаа thread create/destroy -> OS
 *   overhead нь бодит тооцооллоос их гарна. SpeedUp < 1.
 *
 *   Thread pool: thread-үүдийг НЭГ УДАА үүсгэж, дотор нь
 *   condition_variable-аар phase бүрт сэрээнэ.
 *   Thread create/destroy overhead: 0.
 *
 * Thread pool-ийн ажиллах зарчим:
 *   Main thread:                Worker thread t:
 *   -------------------         ----------------------------
 *   phase = 0                   while (running):
 *   notify_all() --------------->  wait(phase шинэчлэгдтэл)
 *                               compareSwap(chunk_t)
 *   wait(бүгд дуустал) <---------  done++
 *   phase++                       notify main
 *   repeat                        wait next phase
 *
 * Pseudo code (зургаас):
 *   BubbleSort(A, n)
 *   {
 *     for k <- 1 to n-1
 *     {
 *       flag <- 0
 *       for i <- 0 to n-k-1        ← зөвхөн эрэмбэлэгдээгүй хэсэг
 *       { if (A[i] > A[i+1])
 *         {
 *           swap(A[i], A[i+1])
 *           flag <- 1
 *         }
 *       }
 *       if (flag == 0) break        ← swap болоогүй → зогсоно
 *     }
 *   }
 *
 * Сайжруулалт:
 *   [1] Sequential: flag-аар early exit — аль хэдийн эрэмбэлэгдсэн
 *       бол үлдсэн давталтыг алгасна (best case O(n))
 *   [2] Parallel:   atomic flag — phase бүрт НЭГЧ swap болоогүй бол
 *       бүх үлдсэн phase-ийг алгасна → хоёр хувилбарт ижил логик
 *
 * Odd-Even алгоритм:
 *   - Even phase: (0,1),(2,3),(4,5)... хосуудыг шалгана
 *   - Odd  phase: (1,2),(3,4),(5,6)... хосуудыг шалгана
 *   - Нэг phase дотор хосууд давхардахгүй → data race байхгүй
 *   - N phase-ийн дараа бүрэн эрэмбэлэгдэнэ
 *
 * Тест хэмжээ: N = 10,000 / 100,000 / 300,000
 *
 * Compile: g++ -O2 -std=c++17 -pthread bubble_sort_threads.cpp -o bubble_sort
 * Run:     ./bubble_sort
 * ============================================================
 */

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <algorithm>
#include <iomanip>
#include <string>
#include <atomic>

using namespace std;
using hrc = chrono::high_resolution_clock;

// ============================================================
// ТУСЛАХ ФУНКЦУУД
// ============================================================

/**
 * Санамсаргүй array үүсгэнэ.
 * @param n Массивын хэмжээ
 * @return n элементийн сортлогдоогүй массив
 */
vector<float> generateRandom(int n) {
    vector<float> arr(n);
    for (int i = 0; i < n; i++)
        arr[i] = static_cast<float>(rand() % 300000);
    return arr;
}

/**
 * Массив эрэмбэлэгдсэн эсэхийг шалгана.
 * @param arr Шалгах массив
 * @return true бол эрэмбэлэгдсэн, false бол эрэмбэлэгдээгүй
 */
bool isSorted(const vector<float>& arr) {
    for (size_t i = 1; i < arr.size(); i++)
        if (arr[i - 1] > arr[i]) return false;
    return true;
}

// ============================================================
// 1. ДАРААЛСАН (SEQUENTIAL) BUBBLE SORT
//    Pseudo code-ийн дагуу хэрэгжүүлсэн
// ============================================================

/**
 * Flag-тай bubble sort — pseudo code-ийн яг хэрэгжүүлэлт.
 *
 * Pseudo code:
 *   for k <- 1 to n-1
 *     flag <- 0
 *     for i <- 0 to n-k-1        (зөвхөн эрэмбэлэгдээгүй хэсэг)
 *       if A[i] > A[i+1]
 *         swap(A[i], A[i+1])
 *         flag <- 1
 *     if flag == 0: break         (swap болоогүй → бүрэн эрэмбэлэгдсэн)
 *
 * Сайжруулалт:
 *   - Гадаад давталт k бүрт сүүлийн k элемент байрандаа → n-k-1 хүртэл л харна
 *   - flag == 0 бол дараагийн давталтууд бүгд хоосон → early exit
 *   - Best case (аль хэдийн эрэмбэлэгдсэн): O(n) — зөвхөн нэг дамжилт
 *   - Worst case: O(n^2)
 *
 * @param arr  Эрэмбэлэх массив
 * @param n    Элементийн тоо
 */
void bubbleSequential(vector<float>& arr, int n) {
    for (int k = 1; k <= n - 1; k++) {      // pseudo: k <- 1 to n-1
        int flag = 0;                         // pseudo: flag <- 0
        for (int i = 0; i <= n - k - 1; i++) { // pseudo: i <- 0 to n-k-1
            if (arr[i] > arr[i + 1]) {        // pseudo: if A[i] > A[i+1]
                swap(arr[i], arr[i + 1]);     // pseudo: swap(A[i], A[i+1])
                flag = 1;                     // pseudo: flag <- 1
            }
        }
        if (flag == 0) break;                 // pseudo: if flag==0 break
    }
}

// ============================================================
// 2. THREAD POOL + ODD-EVEN TRANSPOSITION SORT
//    Flag логикийг atomic-ээр parallel-д хэрэгжүүлсэн
// ============================================================

/**
 * Thread Pool бүтэц.
 *
 * Талбарууд:
 *   arr, n          — эрэмбэлэх array болон хэмжээ
 *   numThreads      — thread-ийн тоо
 *   phase           — одоогийн phase (main thread шинэчилнэ)
 *   totalPairs      — энэ phase-д байх нийт pair
 *   phaseType       — 0=even, 1=odd
 *   done            — phase дуусгасан thread-ийн тоо
 *   flag            — ❗ pseudo code-ийн flag: phase-д swap болсон эсэх
 *   running         — thread-үүд ажиллах эсэхийн флаг
 *
 * Синхрончлол:
 *   cv_work   — main thread worker-уудыг сэрээхэд хэрэглэнэ
 *   cv_done   — worker-ууд main-г мэдэгдэхэд хэрэглэнэ
 *   mtx       — condition_variable-ийн хамгаалалт
 */
struct ThreadPool {
    vector<float>*  arr;
    int             n;
    int             numThreads;

    // Phase мэдээлэл (main thread тохируулна)
    int             phase;
    int             totalPairs;
    int             phaseType;

    // Синхрончлол
    mutex              mtx;
    condition_variable cv_work;   // Worker-уудыг сэрээх
    condition_variable cv_done;   // Бүгд дуусахыг хүлээх
    int                done;      // Дуусгасан thread-ийн тоо

    // ❗ Pseudo code-ийн flag — atomic тул mutex хэрэггүй
    // Worker бүр swap хийвэл flag.store(1) дуудна
    // Main thread phase дараа flag == 0 бол early exit хийнэ
    atomic<int>        flag;

    bool               running;   // false болбол thread-үүд гарна
};

/**
 * Worker thread-ийн гүйцэтгэх ажил.
 *
 * Pseudo code-ийн дотоод давталттай харьцуулал:
 *   Sequential:               Parallel worker:
 *   for i=0 to n-k-1          pairStart..pairEnd chunk
 *     if A[i]>A[i+1]            for i=pairStart to pairEnd
 *       swap(...)                 idx = phaseType + 2*i
 *       flag=1                    if arr[idx]>arr[idx+1]
 *                                   swap(...)
 *                                   pool.flag.store(1)  ← flag=1
 *
 * @param pool      Хуваалцсан thread pool
 * @param threadId  Энэ thread-ийн дугаар (0..numThreads-1)
 */
void workerThread(ThreadPool& pool, int threadId) {
    int lastPhase = -1;

    while (true) {
        // ---- Phase шинэчлэгдтэл хүлээ ----
        {
            unique_lock<mutex> lock(pool.mtx);
            pool.cv_work.wait(lock, [&]{
                return pool.phase != lastPhase || !pool.running;
            });
            if (!pool.running) return;
            lastPhase = pool.phase;
        }

        // ---- Өөрийн chunk-ийн pair-уудыг compare-swap ----
        // Ceiling division-оор chunk хуваана
        int chunkSize = (pool.totalPairs + pool.numThreads - 1) / pool.numThreads;
        int pairStart = threadId * chunkSize;
        int pairEnd   = min(pairStart + chunkSize, pool.totalPairs);

        // idx = phaseType + 2*i томьёо:
        //   phaseType=0 (even): (0,1),(2,3),(4,5)...
        //   phaseType=1 (odd):  (1,2),(3,4),(5,6)...
        for (int i = pairStart; i < pairEnd; i++) {
            int idx = pool.phaseType + 2 * i;
            if (idx < pool.n - 1) {
                if ((*pool.arr)[idx] > (*pool.arr)[idx + 1]) {
                    swap((*pool.arr)[idx], (*pool.arr)[idx + 1]);
                    // ❗ Pseudo code: flag <- 1
                    // atomic store тул mutex хэрэггүй,
                    // бусад thread-тэй race condition байхгүй
                    pool.flag.store(1, memory_order_relaxed);
                }
            }
        }

        // ---- Дуусгасанаа main-д мэдэгдэ ----
        bool last = false;
        {
            unique_lock<mutex> lock(pool.mtx);
            pool.done++;
            if (pool.done == pool.numThreads) last = true;
        }
        if (last) pool.cv_done.notify_one();
    }
}

/**
 * Parallel Odd-Even Transposition Sort — Thread Pool + Flag early exit.
 *
 * Pseudo code-тэй харьцуулал:
 *   Sequential:               Parallel:
 *   for k=1 to n-1            for phase=0 to n-1
 *     flag = 0                  flag.store(0)           ← atomic
 *     for i=0 to n-k-1          notify_all workers
 *       compare-swap              workers: compare-swap chunk
 *       flag=1 if swap            flag.store(1) if swap  ← atomic
 *     if flag==0: break         if flag.load()==0: break ← early exit
 *
 * Thread create/destroy: зөвхөн эхэнд болон сүүлд НЭГ УДАА.
 *
 * @param arr        Эрэмбэлэх array
 * @param n          Элементийн тоо
 * @param numThreads Thread-ийн тоо
 */
void bubbleParallel(vector<float>& arr, int n, int numThreads) {

    ThreadPool pool;
    pool.arr        = &arr;
    pool.n          = n;
    pool.numThreads = numThreads;
    pool.phase      = -1;
    pool.running    = true;
    pool.done       = 0;
    pool.flag.store(0);

    // Thread-үүдийг НЭГ УДАА үүсгэнэ
    vector<thread> threads;
    threads.reserve(numThreads);
    for (int t = 0; t < numThreads; t++)
        threads.emplace_back(workerThread, ref(pool), t);

    // N phase явуулна — pseudo code-ийн гадаад давталттай адил
    for (int phase = 0; phase < n; phase++) {

        {
            lock_guard<mutex> lock(pool.mtx);
            pool.phaseType  = phase % 2;
            pool.totalPairs = (n - pool.phaseType) / 2;
            pool.done       = 0;
            pool.phase      = phase;
            // ❗ Pseudo code: flag <- 0  (phase эхлэхийн өмнө цэвэрлэнэ)
            pool.flag.store(0, memory_order_relaxed);
        }

        // Бүх worker-уудыг сэрээнэ
        pool.cv_work.notify_all();

        // Бүх worker дуустал хүлээнэ (phase barrier)
        {
            unique_lock<mutex> lock(pool.mtx);
            pool.cv_done.wait(lock, [&]{
                return pool.done == pool.numThreads;
            });
        }

        // ❗ Pseudo code: if flag==0 break
        // Энэ phase-д нэгч swap болоогүй → массив бүрэн эрэмбэлэгдсэн
        // Үлдсэн бүх phase-ийг алгасна → sequential-тай адил оновчлол
        if (pool.flag.load(memory_order_relaxed) == 0) break;
    }

    // Thread-үүдийг зогсооно
    {
        lock_guard<mutex> lock(pool.mtx);
        pool.running = false;
    }
    pool.cv_work.notify_all();

    for (auto& t : threads)
        t.join();
}

// ============================================================
// ХЭМЖИЛТ
// ============================================================

/**
 * Benchmark результатын бүтэц.
 *
 * Талбарууд:
 *   seqMs, parMs     — дараалсан болон параллел хугацаа (мс)
 *   speedup          — SpeedUp = seqMs / parMs
 *   efficiency       — Efficiency (%) = speedup / numThreads * 100
 *   totalOps         — нийт гүйцэтгэлийн тоо (n*(n-1)/2)
 *   seqPerf, parPerf — гүйцэтгэлийн хурд (Mops/sec)
 *   dataMB           — массивын хэмжээ (MB)
 *   transferGB       — дамжуулсан өгөгдлийн хэмжээ (GB)
 *   seqBW, parBW     — bandwidth (GB/s)
 *   seqOk, parOk     — эрэмбэлэлт зөв эсэх
 */
struct BenchResult {
    double seqMs;
    double parMs;
    double speedup;
    double efficiency;
    double totalOps;
    double seqPerf;
    double parPerf;
    double dataMB;
    double transferGB;
    double seqBW;
    double parBW;
    bool   seqOk;
    bool   parOk;
};

/**
 * Parallel болон Sequential bubble sort хэмжилт.
 *
 * @param n          Массивын хэмжээ
 * @param numThreads Thread-ийн тоо
 * @return BenchResult — хэмжилтийн үр дүн
 */
BenchResult runBenchmark(int n, int numThreads) {
    vector<float> base   = generateRandom(n);
    vector<float> arrSeq = base;
    vector<float> arrPar = base;

    // Data transfer: n*(n-1)/2 comparison, ~3 elem access тус бүрт
    double elemBytes  = sizeof(float);
    double totalOps   = static_cast<double>(n) * (n - 1) / 2.0;
    double transferGB = totalOps * 3.0 * elemBytes / 1e9;
    double dataMB     = static_cast<double>(n) * elemBytes / (1024.0 * 1024.0);

    auto t0 = hrc::now();
    bubbleSequential(arrSeq, n);
    auto t1 = hrc::now();
    double seqMs = chrono::duration<double, milli>(t1 - t0).count();

    auto t2 = hrc::now();
    bubbleParallel(arrPar, n, numThreads);
    auto t3 = hrc::now();
    double parMs = chrono::duration<double, milli>(t3 - t2).count();

    BenchResult r;
    r.seqMs      = seqMs;
    r.parMs      = parMs;
    r.speedup    = seqMs / parMs;
    r.efficiency = r.speedup / numThreads * 100.0;
    r.totalOps   = totalOps;
    r.seqPerf    = totalOps / (seqMs / 1000.0) / 1e6;
    r.parPerf    = totalOps / (parMs / 1000.0) / 1e6;
    r.dataMB     = dataMB;
    r.transferGB = transferGB;
    r.seqBW      = transferGB / (seqMs / 1000.0);
    r.parBW      = transferGB / (parMs / 1000.0);
    r.seqOk      = isSorted(arrSeq);
    r.parOk      = isSorted(arrPar);
    return r;
}

// ============================================================
// MAIN
// ============================================================
int main() {
    srand(42);

    int numThreads = static_cast<int>(thread::hardware_concurrency());
    if (numThreads == 0) numThreads = 4;

    cout << "+----------------------------------------------------------+\n";
    cout << "|   F.CSM306 — Bubble Sort: Sequential vs std::thread      |\n";
    cout << "|   Odd-Even Sort + Thread Pool (condition_variable)       |\n";
    cout << "|   Hardware threads: " << numThreads
         << "                                    |\n";
    cout << "+----------------------------------------------------------+\n\n";

    // ==========================================================
    // 1: Гүйцэтгэлийн хугацаа болон SpeedUp
    // ==========================================================
    cout << "=== Execution Time & SpeedUp ===\n\n";
    cout << left
         << setw(12) << "N"
         << setw(16) << "Seq (ms)"
         << setw(16) << "Par (ms)"
         << setw(10) << "SpeedUp"
         << setw(14) << "Efficiency"
         << setw(8)  << "SeqOK"
         << setw(8)  << "ParOK"
         << "\n" << string(84, '-') << "\n";

    vector<int> sizes = {10000, 100000, 300000};
    vector<BenchResult> results;

    for (int n : sizes) {
        cout << "  Running N=" << n << "..." << flush;
        BenchResult r = runBenchmark(n, numThreads);
        results.push_back(r);
        cout << "\r" << left
             << setw(12) << n
             << setw(16) << fixed << setprecision(2) << r.seqMs
             << setw(16) << r.parMs
             << setw(10) << setprecision(3) << r.speedup
             << setw(14) << fixed << setprecision(2) << r.efficiency
             << setw(8)  << (r.seqOk ? "YES" : "FAIL")
             << setw(8)  << (r.parOk ? "YES" : "FAIL")
             << "\n";
    }

    // ==========================================================
    // 2: Операциуны тоо болон гүйцэтгэлийн хурд
    // ==========================================================
    cout << "\n=== Operations & Achievable Performance ===\n\n";
    cout << left
         << setw(12) << "N"
         << setw(18) << "Total Ops"
         << setw(18) << "Seq (Mops/s)"
         << setw(18) << "Par (Mops/s)"
         << "\n" << string(66, '-') << "\n";

    for (size_t i = 0; i < sizes.size(); i++) {
        const BenchResult& r = results[i];
        cout << left
             << setw(12) << sizes[i]
             << setw(18) << fixed << setprecision(0) << r.totalOps
             << setw(18) << fixed << setprecision(2) << r.seqPerf
             << setw(18) << r.parPerf
             << "\n";
    }

    // ==========================================================
    // 3: Өгөгдлийн дамжуулалт болон bandwidth
    // ==========================================================
    cout << "\n=== Data Transfer & Bandwidth (RAM <-> Cache) ===\n\n";
    cout << left
         << setw(12) << "N"
         << setw(12) << "Size (MB)"
         << setw(16) << "Transfer (GB)"
         << setw(16) << "Seq BW (GB/s)"
         << setw(16) << "Par BW (GB/s)"
         << "\n" << string(72, '-') << "\n";

    for (size_t i = 0; i < sizes.size(); i++) {
        const BenchResult& r = results[i];
        cout << left
             << setw(12) << sizes[i]
             << setw(12) << fixed << setprecision(2) << r.dataMB
             << setw(16) << setprecision(4) << r.transferGB
             << setw(16) << r.seqBW
             << setw(16) << r.parBW
             << "\n";
    }

    // ==========================================================
    // 4: Thread тооны нөлөө (N=100,000 үед)
    // ==========================================================
    cout << "\n=== Thread Count vs SpeedUp  (N = 100,000) ===\n\n";
    cout << left
         << setw(10) << "Threads"
         << setw(16) << "Par (ms)"
         << setw(12) << "SpeedUp"
         << setw(14) << "Efficiency (%)"
         << "\n" << string(52, '-') << "\n";

    {
        vector<float> ref = generateRandom(100000);
        vector<float> tmp = ref;
        auto r0 = hrc::now();
        bubbleSequential(tmp, 100000);
        auto r1 = hrc::now();
        double seqRef = chrono::duration<double, milli>(r1 - r0).count();

        for (int t : {1, 2, 4, 8, 16}) {
            if (t > numThreads * 4) break;
            vector<float> arr = ref;
            auto p0 = hrc::now();
            bubbleParallel(arr, 100000, t);
            auto p1 = hrc::now();
            double pt = chrono::duration<double, milli>(p1 - p0).count();
            double su = seqRef / pt;
            cout << left
                 << setw(10) << t
                 << setw(16) << fixed << setprecision(2) << pt
                 << setw(12) << setprecision(3) << su
                 << setw(14) << fixed << setprecision(1) << su / t * 100.0
                 << "\n";
        }
    }

    return 0;
}