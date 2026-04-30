/**
 * ---------------------------------------------------------------------
 * F.CSM306 - Параллел ба Хуваарилагдсан тооцоолол
 * Бие даалт: Bubble Sort Алгоритм: — std::thread хувилбар
 * ---------------------------------------------------------------------
 *
 * Алгоритм: Parallel Bubble Sort => 
 * Odd-Even Transposition Sort + Thread Pool 
 *                               (condition_variable barrier)
 *
 * Thread pool:
 *   phase бүрт thread үүсгэж, join() хийхэд N=100,000 үед 
 *   100,000 удаа thread create/destroy хийх нь overhead, 
 *   SpeedUp < 1 зэргээс сэргийлэх шаардлагатай.
 *
 *   Иймд thread-үүдийг НЭГ УДАА үүсгэж, дотор нь
 *   condition_variable-аар phase бүрт сэрээж thread үүсгэж 
 *   устгах зардлыг бууруулсан.
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
 * Pseudo code:
 *   BubbleSort(A, n)
 *   {
 *     for k <- 1 to n-1
 *     {
 *       flag <- 0
 *       for i <- 0 to n-k-1   <--зөвхөн эрэмбэлэгдээгүй хэсэг
 *       { if (A[i] > A[i+1])
 *         {
 *           swap(A[i], A[i+1])
 *           flag <- 1
 *         }
 *       }
 *       if (flag == 0) 
 *            break   <-- swap хийгдээгүй бол үлдсэн давталтыг алгасна
 *     }
 *   }
 *
 * Сайжруулалт:
 *   [1] Sequential: flag-аар early exit - аль хэдийн эрэмбэлэгдсэн
 *       бол үлдсэн давталтыг алгасна (best case O(n))
 *   [2] Parallel: atomic flag - phase бүрт НЭГЧ swap хийгдээгүй бол
 *       бүх үлдсэн phase-ийг алгасна
 *
 * Odd-Even алгоритм:
 *   - Even phase: (0,1),(2,3),(4,5)... хосуудыг шалгана
 *   - Odd  phase: (1,2),(3,4),(5,6)... хосуудыг шалгана
 *   - Нэг phase дотор хосууд давхардахгүй => data race байхгүй
 *   - N phase-ийн дараа бүрэн эрэмбэлэгдэнэ
 *
 * Тест хэмжээ: N = 10,000 / 100,000 / 300,000
 *
 * Compile: g++ -O2 -std=c++17 -pthread multithreads.cpp -o multithreads
 * Run:     ./multithreads
 * ---------------------------------------------------------------------
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

//ТУСЛАХ ФУНКЦУУД
/**
 * Санамсаргүй array үүсгэнэ.
 * @param n Массивын хэмжээ
 * @return n элементийн сортлогдоогүй массив
 */
vector<float> generateRandom(int n)
{
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
bool isSorted(const vector<float>& arr) 
{
    for (size_t i = 1; i < arr.size(); i++)
        if (arr[i - 1] > arr[i]) return false;
    return true;
}



//SEQUENTIAL BUBBLE SORT
/**
 * Flag-тай bubble sort - pseudo code-ийн хэрэгжүүлэлт.
 *
 * Pseudo code:
 *   for k <- 1 to n-1
 *     flag <- 0
 *     for i <- 0 to n-k-1    (зөвхөн эрэмбэлэгдээгүй хэсэг)
 *       if A[i] > A[i+1]
 *         swap(A[i], A[i+1])
 *         flag <- 1
 *     if flag == 0: break     (swap хийгдээгүй буюу бүрэн эрэмбэлэгдсэн)
 *
 *   - n-k-1 хүртэл л шалгах нь оновчтой, аль хэдийн эрэмбэлэгдсэн хэсгийг дахин шалгахгүй
 *   - flag == 0 бол дараагийн давталтууд шаардлагагүй байх тул early exit
 *
 * @param arr Эрэмбэлэх массив
 * @param n Элементийн тоо
 */
void bubbleSequential(vector<float>& arr, int n) 
{
    for (int k = 1; k <= n - 1; k++)
    {
        int flag = 0;
        for (int i = 0; i <= n - k - 1; i++) 
        {
            if (arr[i] > arr[i + 1]) 
            {
                swap(arr[i], arr[i + 1]);
                flag = 1;
            }
        }
        if (flag == 0) break;
    }
}


//PARALLEL BUBBLE SORT
/**
 * Thread Pool бүтэц.
 *
 * Талбарууд:
 *   arr, n - эрэмбэлэх array болон хэмжээ
 *   numThread - thread-ийн тоо
 *   phase - одоогийн phase (main thread шинэчилнэ)
 *   totalPairs - энэ phase-д байх нийт pair
 *   phaseType - 0=even, 1=odd
 *   done - phase дуусгасан thread-ийн тоо
 *   flag - phase-д swap болсон эсэх
 *   running - thread-үүд ажиллах эсэхийГ мэдэгдэх флаг
 *
 * Синхрончлол:
 *   cv_work - main thread worker thread-уудыг сэрээхэд хэрэглэнэ
 *   cv_done - worker-ууд main-д мэдэгдэхэд хэрэглэнэ
 *   mtx - condition_variable-ийн хамгаалалт
 */
struct ThreadPool {
    vector<float>* arr;
    int n;
    int numThreads;

    //Phase мэдээлэл (main thread тохируулна)
    int phase;
    int totalPairs;
    int phaseType;

    //Синхрончлол
    mutex mtx;
    condition_variable cv_work;   //Worker-уудыг сэрээх
    condition_variable cv_done;   //Бүгд дуусахыг хүлээх
    int done;      //Дуусгасан thread-ийн тоо
    //flag нь atomic тул mutex хэрэггүй
    //Worker бүр swap хийвэл flag.store(1) дуудна
    //Main thread phase-ийн дараа flag нь 0 байвал early exit хийнэ
    atomic<int> flag;
    bool running;   //false болбол thread-үүд гарна
};


/**
 * Worker thread-ийн гүйцэтгэх ажил.
 *
 * Pseudo code-ийн дотоод давталттай харьцуулалт:
 *   Sequential:               Parallel worker:
 *   for i=0 to n-k-1          pairStart..pairEnd chunk
 *     if A[i]>A[i+1]            for i=pairStart to pairEnd
 *       swap(...)                 idx = phaseType + 2*i
 *       flag=1                    if arr[idx]>arr[idx+1]
 *                                   swap(...)
 *                                   pool.flag.store(1)     <- flag=1
 *
 * @param pool Дундын thread pool
 * @param threadId Thread-ийн дугаар
 */
void workerThread(ThreadPool& pool, int threadId)
{
    int lastPhase = -1;
    while(true)
    {
        {
            unique_lock<mutex> lock(pool.mtx); //бүх дундын хувьсагчийг хамагаална
            pool.cv_work.wait(lock, [&]{  //phase шинэчлэгдтэл хүлээнэ
                return pool.phase != lastPhase || !pool.running;
            });
            if (!pool.running) 
                return;
            lastPhase = pool.phase; //шинэ phase ажиллаж эхэлсэн гэдгийг мэднэ
        }

        //Ceiling division-оор үлдэгдлийг ч бас хуваана
        int chunkSize = (pool.totalPairs + pool.numThreads - 1) / pool.numThreads;
        int pairStart = threadId * chunkSize;
        int pairEnd = min(pairStart + chunkSize, pool.totalPairs);

        //idx = phaseType + 2i томьёо:
        for (int i = pairStart; i < pairEnd; i++) 
        {
            int idx = pool.phaseType + 2 * i; 
            if (idx < pool.n - 1)
            {
                if ((*pool.arr)[idx] > (*pool.arr)[idx + 1]) //swap хийх эсэхийг шалгана
                {
                    swap((*pool.arr)[idx], (*pool.arr)[idx + 1]);
                    //swap болсон тул flag-ыг 1 болгоно
                    pool.flag.store(1, memory_order_relaxed);
                }
            }
        }

        //Дуусгасанаа main-д мэдэгдэнэ
        bool last = false;
        {
            unique_lock<mutex> lock(pool.mtx);
            pool.done++;
            if (pool.done == pool.numThreads)
                last = true;
        }
        if (last) 
            pool.cv_done.notify_one();
    }
}

/**
 * Parallel Odd-Even Transposition Sort арга
 * Thread Pool
 * Flag
 * 
 * Pseudo code харьцуулалт:
 *   Sequential:               Parallel:
 *   for k=1 to n-1            for phase=0 to n-1
 *     flag = 0                  flag.store(0)           <- atomic
 *     for i=0 to n-k-1          notify_all workers
 *       compare-swap              workers: compare-swap chunk
 *       flag=1 if swap            flag.store(1) if swap         <- atomic
 *     if flag==0: break         if flag.load()==0: break        <-early exit
 *
 * @param arr Эрэмбэлэх array
 * @param n Элементийн тоо
 * @param numThreads Thread-ийн тоо
 */
void bubbleParallel(vector<float>& arr, int n, int numThreads)
{
    ThreadPool pool;
    pool.arr = &arr;
    pool.n = n;
    pool.numThreads = numThreads;
    pool.phase = -1;
    pool.running = true;
    pool.done = 0;
    pool.flag.store(0);

    //Thread creation
    vector<thread> threads;
    threads.reserve(numThreads);
    for (int t = 0; t < numThreads; t++)
        threads.emplace_back(workerThread, ref(pool), t);

    //Main control loop
    for (int phase = 0; phase < n; phase++)
    {
        {
            lock_guard<mutex> lock(pool.mtx); //бүх дундын хувьсагчийг хамагаална
            pool.phaseType = phase%2; // even/odd phase тодорхойлоно
            pool.totalPairs = (n - pool.phaseType)/2; // энэ phase-д байх нийт pair
            pool.done = 0; //энэ phase-д дуусгасан thread-ийн тоог reset хийнэ
            pool.phase = phase; //worker-ууд ямар phase дээр ажиллаж буйг мэднэ
            //phase эхлэхийн өмнө flag-ыг цэвэрлэнэ
            pool.flag.store(0, memory_order_relaxed);
        }

        //Бүх worker-уудыг сэрээнэ
        pool.cv_work.notify_all();

        //Бүх worker дуустал хүлээх phase barrier
        {
            unique_lock<mutex> lock(pool.mtx);
            pool.cv_done.wait(lock, [&]{
                return pool.done == pool.numThreads;
            });
        }

        //Энэ phase-д нэгч swap гараагүй бол массив эрэмбэлэгдсэн гэж үзнэ. Үлдсэн бүх phase-ийг алгасна → sequential-тай адил оновчлол
        if (pool.flag.load(memory_order_relaxed) == 0) 
            break;
    }

    //Thread-үүдийг зогсоож, нэгд нэгэнгүй join хийнэ
    {
        lock_guard<mutex> lock(pool.mtx);
        pool.running = false;
    }
    pool.cv_work.notify_all();

    for (auto& t : threads)
        t.join();
}

//ХЭМЖИЛТ
/**
 * Benchmark үр дүнгийн бүтэц.
 *
 *   seqMs, parMs - дараалсан болон параллел хугацаа (мс)
 *   speedup - SpeedUp = seqMs / parMs
 *   efficiency - Efficiency (%) = speedup / numThreads * 100
 *   totalOps - нийт гүйцэтгэлийн тоо (n*(n-1)/2)
 *   seqPerf, parPerf - гүйцэтгэлийн хурд (Mops/sec)
 *   dataMB - массивын хэмжээ (MB)
 *   transferGB - дамжуулсан өгөгдлийн хэмжээ (GB)
 *   seqBW, parBW - bandwidth (GB/s)
 *   seqOk, parOk - эрэмбэлэлт зөв эсэх
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
    bool seqOk;
    bool parOk;
};

/**
 * Parallel болон Sequential bubble sort хэмжилт.
 *
 * @param n Массивын хэмжээ
 * @param numThreads Thread-ийн тоо
 * @return BenchResult - хэмжилтийн үр дүн
 */
BenchResult runBenchmark(int n, int numThreads) {
    vector<float> base   = generateRandom(n);
    vector<float> arrSeq = base;
    vector<float> arrPar = base;

    double elemBytes  = sizeof(float);
    double totalOps   = static_cast<double>(n) * (n - 1) / 2.0;
    double transferGB = totalOps * 3.0 * elemBytes / 1e9;
    double dataMB     = static_cast<double>(n) * elemBytes / (1024.0 * 1024.0);

    //sequential
    auto t0 = hrc::now();
    bubbleSequential(arrSeq, n);
    auto t1 = hrc::now();
    double seqMs = chrono::duration<double, milli>(t1 - t0).count();

    //parallel
    auto t2 = hrc::now();
    bubbleParallel(arrPar, n, numThreads);
    auto t3 = hrc::now();
    double parMs = chrono::duration<double, milli>(t3 - t2).count();

    BenchResult r;
    r.seqMs = seqMs;
    r.parMs = parMs;
    r.speedup = seqMs / parMs;
    r.efficiency = r.speedup / numThreads * 100.0;
    r.totalOps = totalOps;
    r.seqPerf = totalOps / (seqMs / 1000.0) / 1e6;
    r.parPerf = totalOps / (parMs / 1000.0) / 1e6;
    r.dataMB = dataMB;
    r.transferGB = transferGB;
    r.seqBW = transferGB / (seqMs / 1000.0);
    r.parBW = transferGB / (parMs / 1000.0);
    r.seqOk = isSorted(arrSeq);
    r.parOk = isSorted(arrPar);
    return r;
}

//MAIN
int main() {
    srand(42);

    int numThreads = static_cast<int>(thread::hardware_concurrency());
    if (numThreads == 0) numThreads = 4;

    cout << "+----------------------------------------------------------+\n";
    cout << "|   F.CSM306 — Bubble Sort: Sequential vs std::thread      |\n";
    cout << "|   Hardware threads: " << numThreads
         << "                                    |\n";
    cout << "+----------------------------------------------------------+\n\n";

    //Гүйцэтгэлийн хугацаа болон SpeedUp
    cout << "---+--- Execution Time & SpeedUp ---+---\n\n";
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

    for (int n : sizes)
    {
        cout << " => Running N=" << n << "..." << flush;
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

    //Нийт гүйцэтгэсэн ажлын тоо болон хурд
    cout << "\n---+--- Operations & Achievable Performance ---+---\n\n";
    cout << left
         << setw(12) << " N"
         << setw(18) << "Total Ops"
         << setw(18) << "Seq (Mops/s)"
         << setw(18) << "Par (Mops/s)"
         << "\n" << string(66, '-') << "\n";

    for (size_t i = 0; i < sizes.size(); i++)
    {
        const BenchResult& r = results[i];
        cout << left
             << setw(12) << sizes[i]
             << setw(18) << fixed << setprecision(0) << r.totalOps
             << setw(18) << fixed << setprecision(2) << r.seqPerf
             << setw(18) << r.parPerf
             << "\n";
    }

    //Өгөгдөл дамжуулалт болон bandwidth
    cout << "\n---+--- Data Transfer & Bandwidth /RAM <-> Cache/ ---+---\n\n";
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

    //Thread-ийн тооны хамаарал (N=10,000 үед)
    cout << "\n---+ Thread Count vs SpeedUp  (N = 10,000) +---\n\n";
    cout << left
         << setw(10) << "Threads"
         << setw(16) << "Par (ms)"
         << setw(12) << "SpeedUp"
         << setw(14) << "Efficiency (%)"
         << "\n" << string(52, '-') << "\n";

        {
            const int BENCH_N = 10000;
            const int RUNS = 3;

            {
                vector<float> w = generateRandom(BENCH_N);
                bubbleSequential(w, BENCH_N);
                bubbleParallel(w, BENCH_N, numThreads);
            }
            //sequential
            double st = 0;
            for (int i = 0; i < RUNS; i++)
            {
                vector<float> tmp = generateRandom(BENCH_N);
                auto t0 = hrc::now();
                bubbleSequential(tmp, BENCH_N);
                auto t1 = hrc::now();
                st += chrono::duration<double, milli>(t1 - t0).count();
            }
            st /= RUNS;
            //parallel
            for (int t : {2, 4, 8, 16})
            {
                if (t > numThreads*2)
                    break; //хэт олон thread үүсгэхээс сэргийлэх

                double pt = 0;
                for (int i = 0; i < RUNS; i++)
                {
                    vector<float> arr = generateRandom(BENCH_N);
                    auto p0 = hrc::now();
                    bubbleParallel(arr, BENCH_N, t);
                    auto p1 = hrc::now();
                    pt += chrono::duration<double, milli>(p1 - p0).count();
                }
                pt /= RUNS;

                double su = st / pt;
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