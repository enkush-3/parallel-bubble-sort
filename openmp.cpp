#include <bits/stdc++.h>
#include <omp.h>

using namespace std;

void bubbleOpenMP(vector<float>& a, int n){

    for(int i = 0; i < n; i++){

        int start = i % 2;

        #pragma omp parallel for schedule(static)
        for(int j = start; j < n - 1; j += 2){

            if(a[j] > a[j+1]){
                float temp = a[j];
                a[j] = a[j+1];
                a[j+1] = temp;
            }
        }
    }
}

int main(){

    int N = 100000;
    vector<float> arr(N);

    for(int i = 0; i < N; i++){
        arr[i] = rand() % 100000;
    }

    double start = omp_get_wtime();
    bubbleOpenMP(arr, N);
    double end = omp_get_wtime();
    cout << "OpenMP Time: " << (end - start) << " sec" << endl;

    bool sorted = true;
    for(int i = 0; i < N - 1; i++){
        if(arr[i] > arr[i+1]){
            sorted = false;
            break;
        }
    }

    cout << "Sorted: " << (sorted ? "YES" : "NO") << endl;

    return 0;
}