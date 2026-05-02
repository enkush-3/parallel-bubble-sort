#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>
#include <string>
#include <omp.h>
#include <fstream>
#include <algorithm>

using namespace std;

// Тоог мянгатын орноор таслах
string fmt(long long n) {
    string s = to_string(n);
    int p = s.size() - 3;
    while (p > 0) { s.insert(p, ","); p -= 3; }
    return s;
}

// Өгөгдлийн хэмжээг форматлах
string sizeStr(double mb) {
    ostringstream os;
    if (mb >= 1024) os << fixed << setprecision(2) << mb/1024 << " GB";
    else if (mb >= 1) os << fixed << setprecision(2) << mb << " MB";
    else os << fixed << setprecision(2) << mb*1024 << " KB";
    return os.str();
}

// Sequential Bubble Sort
double seqSortTime(vector<int>& a) {
    int n = a.size();
    auto t0 = chrono::high_resolution_clock::now();
    for (int i = 0; i < n-1; i++) {
        bool swapped = false;
        for (int j = 0; j < n-1-i; j++) {
            if (a[j] > a[j+1]) {
                swap(a[j], a[j+1]);
                swapped = true;
            }
        }
        if (!swapped) break;
    }
    auto t1 = chrono::high_resolution_clock::now();
    return chrono::duration<double, milli>(t1 - t0).count();
}

// Parallel Bubble Sort (OpenMP Odd-Even Transposition)
struct ParResult {
    double exec_ms;
    long long cmp, swp;
    bool ok;
};

ParResult parSort(vector<int>& a) {
    int n = a.size();
    long long total_cmp = 0, total_swp = 0;
    bool is_sorted = false;

    auto t0 = chrono::high_resolution_clock::now();
    for (int phase = 0; phase < n && !is_sorted; phase += 2) {
        long long cmp_even = 0, swp_even = 0;
        long long cmp_odd = 0, swp_odd = 0;

        #pragma omp parallel for reduction(+:cmp_even, swp_even) schedule(static)
        for (int j = 0; j < n - 1; j += 2) {
            cmp_even++;
            if (a[j] > a[j + 1]) {
                swap(a[j], a[j + 1]);
                swp_even++;
            }
        }

        #pragma omp parallel for reduction(+:cmp_odd, swp_odd) schedule(static)
        for (int j = 1; j < n - 1; j += 2) {
            cmp_odd++;
            if (a[j] > a[j + 1]) {
                swap(a[j], a[j + 1]);
                swp_odd++;
            }
        }

        total_cmp += cmp_even + cmp_odd;
        total_swp += swp_even + swp_odd;

        if (swp_even == 0 && swp_odd == 0) {
            is_sorted = true;
        }
    }
    auto t1 = chrono::high_resolution_clock::now();
    double exec_ms = chrono::duration<double, milli>(t1 - t0).count();
    
    bool ok = is_sorted_until(a.begin(), a.end()) == a.end();
    
    return {exec_ms, total_cmp, total_swp, ok};
}

// Үр дүн хадгалах бүтэц (Яг thread хувилбартай ижил)
struct FullResult {
    int N;
    double seq_time_ms;
    double par_time_ms;
    double comp_time_s, comp_time_p;
    double transfer_time_ms;
    double data_MB;
    long long ops_s, ops_p;
    double mops_s, mops_p;
    double speedup;
    double efficiency;
    bool correct_s, correct_p;
    int threads;
};

// Хэмжилт хийх функц
FullResult runExperiment(int N, int threads) {
    vector<int> data_s(N), data_p(N);
    mt19937 gen(42); 
    uniform_int_distribution<int> dist(0, 1000000);
    
    for (int i = 0; i < N; i++) {
        int val = dist(gen);
        data_s[i] = val;
        data_p[i] = val;
    }
    
    // Sequential
    double seq_time = seqSortTime(data_s);
    bool ok_s = is_sorted_until(data_s.begin(), data_s.end()) == data_s.end();
    
    // Parallel (OpenMP)
    ParResult pr = parSort(data_p);
    bool ok_p = pr.ok;
    
    // Метрик тооцооллууд (Thread-тэй яг ижил)
    long long ops_s = (long long)N * N;          
    long long ops_p = pr.cmp + pr.swp * 3;
    
    double read_bytes = (double)(pr.cmp * 2 + pr.swp * 2) * 4.0;
    double write_bytes = (double)(pr.swp * 2) * 4.0;
    double total_bytes = read_bytes + write_bytes;
    double data_MB = total_bytes / (1024.0 * 1024.0);
    
    double bandwidth_GBps = 20.0;
    double transfer_time_ms = (total_bytes / (bandwidth_GBps * 1e9)) * 1000.0;
    
    double comp_time_s = max(0.01, seq_time - transfer_time_ms);
    double comp_time_p = max(0.01, pr.exec_ms - transfer_time_ms);
    
    double mops_s = (ops_s / 1e6) / (seq_time / 1000.0);
    double mops_p = (ops_p / 1e6) / (pr.exec_ms / 1000.0);
    double speedup = seq_time / pr.exec_ms;
    double efficiency = (speedup / threads) * 100.0;
    
    return {N, seq_time, pr.exec_ms, comp_time_s, comp_time_p, transfer_time_ms,
            data_MB, ops_s, ops_p, mops_s, mops_p, speedup, efficiency,
            ok_s, ok_p, threads};
}

// Хүснэгтээр хэвлэх
void printTable(const FullResult& r) {
    const int col1 = 32, col2 = 20, col3 = 20;
    string sep(col1+col2+col3, '=');
    string line(col1+col2+col3, '-');
    
    cout << "\n" << sep << "\n";
    cout << "  Өгөгдлийн хэмжээ N = " << fmt(r.N) << " | Ажилласан урсгал: " << r.threads << "\n";
    cout << sep << "\n";
    cout << left << setw(col1) << "МЕТРИК (Үнэлгээ)"
         << setw(col2) << "Sequential (Хуучин)"
         << setw(col3) << "Parallel (OpenMP)" << "\n";
    cout << line << "\n";
    
    auto d2str = [](double ms, const string& unit) {
        ostringstream os; os << fixed << setprecision(2) << ms << " " << unit; return os.str();
    };
    
    cout << setw(col1) << "Execution time" << setw(col2) << d2str(r.seq_time_ms, "ms") << setw(col3) << d2str(r.par_time_ms, "ms") << "\n";
    cout << setw(col1) << "Computation time" << setw(col2) << d2str(r.comp_time_s, "ms") << setw(col3) << d2str(r.comp_time_p, "ms") << "\n";
    cout << setw(col1) << "Data transfer time" << setw(col2) << d2str(r.transfer_time_ms, "ms") << setw(col3) << d2str(r.transfer_time_ms, "ms") << "\n";
    cout << line << "\n";
    cout << setw(col1) << "Amount of data" << setw(col2) << sizeStr(r.data_MB) << setw(col3) << sizeStr(r.data_MB) << "\n";
    cout << setw(col1) << "Total operations" << setw(col2) << fmt(r.ops_s) << setw(col3) << fmt(r.ops_p) << "\n";
    cout << setw(col1) << "Achievable performance" << setw(col2) << d2str(r.mops_s, "MOPS") << setw(col3) << d2str(r.mops_p, "MOPS") << "\n";
    cout << setw(col1) << "Correctness" << setw(col2) << (r.correct_s ? "Yes" : "NO!") << setw(col3) << (r.correct_p ? "Yes" : "NO!") << "\n";
    cout << sep << "\n";
    cout << "  ХАРЬЦУУЛАЛТ / ДҮГНЭЛТ: \n";
    cout << "  SpeedUp (Хурдсалт)      : " << fixed << setprecision(2) << r.speedup << "x\n";
    cout << "  Efficiency (Үр ашиг)    : " << fixed << setprecision(1) << r.efficiency << "%\n";
    cout << "  Цаг хэмнэлт             : " << r.seq_time_ms - r.par_time_ms << " ms\n";
    cout << sep << "\n";
}

// CSV руу хадгалах
void saveToCSV(const vector<FullResult>& results, const string& fname) {
    ofstream f(fname);
    if (!f) {
        cerr << "CSV файл нээхэд алдаа: " << fname << endl;
        return;
    }
    // ЯГ THREAD CSV ФОРМАТТАЙ ИЖИЛ
    f << "N,seq_time_ms,par_time_ms,comp_time_s,comp_time_p,transfer_time_ms,data_MB,"
         "ops_s,ops_p,mops_s,mops_p,speedup,efficiency_%,correct_s,correct_p,threads\n";
    for (const auto& r : results) {
        f << r.N << "," << fixed << setprecision(6) << r.seq_time_ms << "," << r.par_time_ms << ","
          << r.comp_time_s << "," << r.comp_time_p << "," << r.transfer_time_ms << "," << r.data_MB << ","
          << r.ops_s << "," << r.ops_p << "," << r.mops_s << "," << r.mops_p << "," << r.speedup << ","
          << r.efficiency << "," << r.correct_s << "," << r.correct_p << "," << r.threads << "\n";
    }
    f.close();
    cout << "\n[CSV] Үр дүнг хадгаллаа: " << fname << endl;
}

int main() {
    int threads = omp_get_max_threads();
    cout << "\n   АЛГОРИТМЫН ХАРЬЦУУЛАЛТ: Sequential vs OpenMP (БОДИТ ХЭМЖИЛТ)   \n";
    cout << "   Боломжит урсгалын тоо (Threads): " << threads << "\n";
    
    // N-ийн утгууд (Thread дээрхтэй адил)
    vector<int> sizes = {5000, 10000, 50000, 100000}; 
    vector<FullResult> results;
    
    for (size_t i = 0; i < sizes.size(); i++) {
        int N = sizes[i];
        cout << "\n[" << i+1 << "/" << sizes.size() << "] Ажиллаж байна... N=" << fmt(N) << endl;
        
        FullResult r = runExperiment(N, threads);
        printTable(r);
        results.push_back(r);
    }
    
    saveToCSV(results, "openmp_results.csv");
    cout << "\nБүх тест амжилттай дууслаа!\n";
    return 0;
}


/*

g++ -O3 -fopenmp openmp.cpp -o openmp
./openmp
python plot.py

*/