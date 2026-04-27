/**
 * =============================================================
 *  F.CSM306 - Параллел ба Хуваарилагдсан Тооцоолол
 *  Бие даалт: Bubble Sort - Multi Thread (std::thread) хувилбар
 * =============================================================
 *
 *  Аргачлал:
 *    1. Массивыг thread-үүдийн тоогоор тэнцүү хэсгүүдэд хувааж,
 *       тус бүр дэх хэсэгт Sequential Bubble Sort хийнэ.
 *    2. Бүх thread дуусмагц хэсгүүдийг нэгтгэх (merge) замаар
 *       эцсийн эрэмбэлэгдсэн массив бүрдэнэ.
 * =============================================================
 */

#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <algorithm>
#include <random>
#include <iomanip>
#include <cassert>

// ====================================================
//  Туслах буюу хугацаа хэмжих классууд болон функцууд
// ====================================================
using Clock     = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Ms        = std::chrono::duration<double, std::milli>;

static double elapsed_ms(TimePoint start, TimePoint end) {
    return std::chrono::duration_cast<Ms>(end - start).count();
}

// ==========================
// 1. Segmented Bubble Sort
//     arr  — бүтэн массив
//     lo   — эхлэх индекс
//     hi   — дуусах индекс
// ==========================
void bubbleSortSegment(std::vector<int>& arr, int lo, int hi) {
    //Классик bubble sort
    for (int i = lo; i <= hi; ++i) {
        bool swapped = false;
        for (int j = lo; j < hi - (i - lo); ++j) {
            if (arr[j] > arr[j + 1]) {
                std::swap(arr[j], arr[j + 1]);
                swapped = true;
            }
        }
        //Swap байхгүй бол хэсэг аль хэдийн эрэмбэлэгдсэн гэж үзнэ
        if (!swapped) break;
    }
}

// =======================================
// 2. Хоёр эрэмбэлэгдсэн хэсгийг нэгтгэнэ
// arr[lo..mid] болон arr[mid+1..hi]
// =======================================
void mergeSegments(std::vector<int>& arr, int lo, int mid, int hi) {
    //Зүүн хэсгийг буферт хуулна (баруун хэсэг шууд arr-д байгаа)
    std::vector<int> left(arr.begin() + lo, arr.begin() + mid + 1);

    int i = 0;   //зүүн буфер дэх заагч
    int j = mid + 1;    //баруун хэсгийн заагч
    int k = lo;      //үр дүн бичих байрлал

    while (i < (int)left.size() && j <= hi) {
        if (left[i] <= arr[j]) {
            arr[k++] = left[i++];
        } else {
            arr[k++] = arr[j++];
        }
    }
    //Үлдсэн зүүн хэсгийг arr-д буцааж бичнэ
    while (i < (int)left.size()) arr[k++] = left[i++];
    //arr[j..hi] буюу баруун хэсэг аль хэдийн arr-д байгаа
}

// ============================================================
// 3. Параллел Bubble Sort (std::thread)
// ============================================================
void parallelBubbleSort(std::vector<int>& arr, int numThreads) {
    int n = static_cast<int>(arr.size());
    if (n <= 1) return;

    //Thread-ийн тоог массивын хэмжээтэй тохируулах
    numThreads = std::min(numThreads, n);

    //Тэгш бус хуваалтанд зориулан ceiling хуваалт хийнэ
    std::vector<std::pair<int,int>> ranges(numThreads); // {lo, hi} индексийн хосууд
    int segSize = (n + numThreads - 1) / numThreads;  // ceiling division

    //Thread тус бүрт хуваах хэсгийн эхлэх ба дуусах индексийг тооцно
    for (int t = 0; t < numThreads; ++t) 
    {
        int lo = t * segSize;
        int hi = std::min(lo + segSize - 1, n - 1);
        ranges[t] = {lo, hi};
    }

    //Thread тус бүрт bubble sort ажиллуулах
    std::vector<std::thread> threads(numThreads);

    for (int t = 0; t < numThreads; ++t)
    {
        auto [lo, hi] = ranges[t];
        threads[t] = std::thread([&arr, lo, hi]() {
            bubbleSortSegment(arr, lo, hi);
        });
    }

    //Бүх thread дуусахыг хүлээнt
    for (auto& th : threads) {
        th.join();  // main thread нь бусад thread дуусахыг хүлээнэ
    }

    //Эрэмбэлэгдсэн хэсгүүдийг нэгтгэнэв Bottom-up merge: 1 хэсэг -> 2 хэсэг -> 4 хэсэг -> ...
    int active = numThreads;  //одоогийн эрэмбэлэгдсэн хэсгүүдийн тоо

    while (active > 1) {
        int newActive = 0; //дараагийн эрэмбэлэгдсэн хэсгүүдийн тоо

        for (int i = 0; i + 1 < active; i += 2)
        {
            int lo  = ranges[i].first; //эхлэх индекс
            int mid = ranges[i].second; //нэгтгэхэд ашиглах дунд индекс буюу эхний хэсгийн төгсгөл
            int hi  = ranges[i + 1].second; //дуусах индекс буюухоёр дахь хэсгийн төгсгөл

            mergeSegments(arr, lo, mid, hi); //хоёр эрэмбэлэгдсэн хэсгийг нэгтгэнэ

            ranges[newActive++] = {lo, hi}; //нэгтгэсэн хэсгийн шинэ эхлэх ба дуусах индексийг хадгална
        }

        if (active % 2 != 0) //Сондгой хэсэг үлдвэл merge хийхгүй хадгална
        {
            ranges[newActive++] = ranges[active - 1];
        }

        active = newActive;
    }
}

// ====================================================
// 4. Sequential Bubble Sort (харьцуулалтад зориулсан)
// ====================================================
void sequentialBubbleSort(std::vector<int>& arr) {
    int n = static_cast<int>(arr.size());
    for (int i = 0; i < n - 1; ++i) {
        bool swapped = false;
        for (int j = 0; j < n - i - 1; ++j) {
            if (arr[j] > arr[j + 1]) {
                std::swap(arr[j], arr[j + 1]);
                swapped = true;
            }
        }
        if (!swapped) break;
    }
}

// ======================
// 5. Тест болон хэмжилт
// ======================

//Санамсаргүй массив үүсгэх
std::vector<int> generateRandom(int n, int seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 1000000);
    std::vector<int> arr(n);
    for (auto& x : arr) x = dist(rng);
    return arr;
}

//Массив эрэмбэлэгдсэн эсэхийг шалгана
bool isSorted(const std::vector<int>& arr) {
    for (int i = 0; i + 1 < (int)arr.size(); ++i) {
        if (arr[i] > arr[i + 1]) return false;
    }
    return true;
}

//Хэмжилт
struct BenchResult {
    double seqMs; //sequential гүйцэтгэлийн хугацаа
    double parMs; //parallel гүйцэтгэлийн хугацаа
    double speedup; //SpeedUp = seqMs / parMs
    double efficiency; //Efficiency = speedup / numThreads * 100 (%)
    double seqBW; //sequential bandwidth (GB/s)
    double parBW; //parallel bandwidth (GB/s)
    double transferGB; //нийт дамжуулсан өгөгдлийн хэмжээ (GB)
};

BenchResult benchmark(int n, int numThreads) {
    std::vector<int> base = generateRandom(n);

    //Дамжуулсан өгөгдлийн хэмжээ (GB) болон түүнээс тооцсон bandwidth (GB/s)
    double dataSizeBytes = static_cast<double>(n) * sizeof(int);
    double transferGB    = dataSizeBytes * n / 1e9;

    
    {
        //Sequential
        std::vector<int> arr = base; //өгөгдлийг хуулж авна
        auto tseq1 = Clock::now();
        sequentialBubbleSort(arr);
        auto tseq2 = Clock::now();

        //Зөв эрэмбэлэгдсэн эсэхийг баталгаажуулна
        assert(isSorted(arr) && "Sequential sort failed");

        BenchResult r;
        r.transferGB = transferGB;
        
        r.seqMs = elapsed_ms(tseq1, tseq2);
        r.seqBW = transferGB/(r.seqMs/1000.0);

        //Parallel
        std::vector<int> arr2 = base; //өгөгдлийг дахин хуулж авна
        auto tpar1 = Clock::now();
        parallelBubbleSort(arr2, numThreads);
        auto tpar2 = Clock::now();

        assert(isSorted(arr2) && "Parallel sort failed");

        r.parMs = elapsed_ms(tpar1, tpar2);
        r.parBW = transferGB/(r.parMs/1000.0);
        
    
        if (r.parMs > 0) //0 буюу алдаа гарсан тохиолдолд SpeedUp-ыг бодохгүй
        {
            r.speedup = r.seqMs / r.parMs; // SpeedUp = seqMs / parMs
        } else {
                r.speedup = 0.0;
        }
        
        r.efficiency = r.speedup / numThreads * 100.0;
        return r;
    }
}

// =====
// main 
// =====
int main() {
    //Hardware thread-үүдийн тоог тодорхойлно
    unsigned int hwThreads = std::thread::hardware_concurrency();
    if (hwThreads == 0) {
        hwThreads = 4;  //мэдэгдэхгүй бол 4 гэж тооцно
    }
    std::cout << "+-------------------------------------------------+\n";
    std::cout << "|  F.CSM306 - Parallel Bubble Sort (Multi Thread) |\n";
    std::cout << "|  Hardware threads: " << hwThreads << "                            |\n";
    std::cout << "+------------------------------------------------ +\n\n";

    //Bubble Sort нь O(n²) учраас 1M дээр sequential маш удаан байх учраас 1M дээр зөвхөн parallel туршив
    std::vector<int> sizes = {10000, 100000};

    std::cout << "+----------------------------------------------------------------------+\n";
    std::cout << std::left
              << std::setw(12) << " N"
              << std::setw(18) << "Sequential (ms)"
              << std::setw(18) << "Parallel (ms)"
              << std::setw(12) << "SpeedUp"
              << std::setw(14) << "Efficiency (%)"
              << std::setw(12) << "Threads"
              << "\n";
    std::cout << std::string(86, '-') << "\n";

    for (int n : sizes) {
        BenchResult r = benchmark(n, hwThreads);
        std::cout << std::left
                  << std::setw(12) << n
                  << std::setw(18) << std::fixed << std::setprecision(2) << r.seqMs
                  << std::setw(18) << std::fixed << std::setprecision(2) << r.parMs
                  << std::setw(12) << std::fixed << std::setprecision(3) << r.speedup
                  << std::setw(14) << std::fixed << std::setprecision(1) << r.efficiency
                  << std::setw(12) << hwThreads
                  << "\n";

        //Data Transfer хэмжилт
        std::cout << std::string(4, ' ')
                  << "Transfer: " << std::setprecision(3) << r.transferGB << " GB"
                  << "  |  Seq BW: " << std::setprecision(2) << r.seqBW << " GB/s"
                  << "  |  Par BW: " << r.parBW << " GB/s\n\n";
    }

    std::cout << "+----------------------------------------------------------------------+\n";

    //1000000 дээр зөвхөн parallel-аар ажиллах
    {
        int n = 1000000;
        std::vector<int> arr = generateRandom(n);

        auto t0 = Clock::now();
        parallelBubbleSort(arr, hwThreads);
        auto t1 = Clock::now();

        double parMs     = elapsed_ms(t0, t1);
        double transferGB = static_cast<double>(n) * sizeof(int) * n / 1e9;
        double parBW      = transferGB / (parMs / 1000.0);  // GB/s

        assert(isSorted(arr));

        std::cout << std::left
                  << std::setw(12) << "1,000,000"
                  << std::setw(18) << "(хэт удаан)"
                  << std::setw(18) << std::fixed << std::setprecision(2) << parMs
                  << std::setw(12) << "-"
                  << std::setw(14) << "-"
                  << std::setw(12) << hwThreads
                  << "\n";

        std::cout << std::string(4, ' ')
                  << "Transfer: " << std::setprecision(3) << transferGB << " GB"
                  << "  |  Par BW: " << std::setprecision(2) << parBW << " GB/s\n\n";
    }
    std::cout << "+----------------------------------------------------------------------+\n";

    std::cout << "\nБүх тестийг амжилттай давсан. Эрэмбэлэлт зөв.\n\n";

    //Thread-ийн тооны нөлөөг судлана
    std::cout << "Thread тооны нөлөө N = 10,000 үед:\n";
    std::cout << std::string(60, '-') << "\n";
    std::cout << std::setw(12) << "Threads"
              << std::setw(18) << "Parallel (ms)"
              << std::setw(12) << "SpeedUp"
              << std::setw(14) << "Efficiency (%)\n";
    std::cout << std::string(60, '-') << "\n";

    for (int t : {2, 4, 8, 16}) {
        if (t > (int)hwThreads * 2) break; //Хэт олон thread үүсгэхээс сэргийлэх
        BenchResult r = benchmark(10'000, t);
        std::cout << std::setw(12) << t
                  << std::setw(18) << std::fixed << std::setprecision(2) << r.parMs
                  << std::setw(12) << std::fixed << std::setprecision(3) << r.speedup
                  << std::fixed << std::setprecision(1) << r.efficiency << "%\n";
    }

    std::cout << "\n+--------------------------------------+\n";
    return 0;
}
