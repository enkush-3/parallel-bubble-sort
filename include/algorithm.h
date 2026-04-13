#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <vector>

// 1. Энгийн дараалсан хувилбар (Sequential)
void sortSequential(std::vector<int>& arr);

// 2. C++ стандарт сангийн олон урсгалт хувилбар (std::thread)
void sortStdThread(std::vector<int>& arr);

// 3. OpenMP ашигласан хувилбар
void sortOpenMP(std::vector<int>& arr);

// 4. CUDA ашигласан GPU хувилбар
void sortCUDA(std::vector<int>& arr);

#endif // ALGORITHM_H
