## 📁 Төслийн Файлын Бүтэц (Project File Structure)

### ✅ Үүсгэсэн Үндсэн Файлууд:

1. **Root Configuration:**
   * `CMakeLists.txt` - Төслийн build configuration
   * `Makefile` - Энгийн compilation script
   * `.gitignore` - Git ignore файл
2. **Header Files (include/):**
   * `algorithm.h` - Үндсэн Algorithm интерфэйс (4 хувилбарын нэгдсэн)
   * `timer.h` - Гүйцэтгэлийн хугацаа хэмжих utility
3. **Source Code (src/):**
   * `sequential.cpp` - А гишүүн: Дараалсан хувилбар
   * `openmp.cpp` - А гишүүн: OpenMP параллел хувилбар
   * `threading.cpp` - В гишүүн: std::thread параллел хувилбар
   * `cuda_kernel.cu` - Б гишүүн: CUDA GPU kernels
   * `cuda_wrapper.cpp` - Б гишүүн: CUDA C++ wrapper
   * `src/CMakeLists.txt` - src модулийн build config
4. **Benchmark (benchmark/):**
   * `benchmark_main.cpp` - 10K, 100K, 1M элементүүдтэй үндсэн benchmark
   * `benchmark/CMakeLists.txt` - Benchmark build config
5. **Tests (tests/):**
   * `test_correctness.cpp` - Үнэн зөвийг шалгах тест
   * `test_performance.cpp` - Гүйцэтгэлийн тест
   * `tests/CMakeLists.txt` - Tests build config
6. **Documentation (docs/):**
   * `README.md` - Төслийн бүтэн ашиглалтын зааваз

---

## 🚀 Ашиглалтын Заавар:

### Компайл хийх:

```bash
cd parallel-algorithms
make build
```

### Benchmark ажилуулах:

```bash
make run-benchmark
```

### Тестүүдийг явуулах:

```bash
make run-tests
```

### Бүхэлээр явуулах:

```bash
make run-all
```

---

# CMake

```cpp
cmake_minimum_required(VERSION 3.18)

# CUDA-г дэмждэгээр төслийг үүсгэх

project(ParallelSorting LANGUAGES CXX CUDA)

# C++ стандартыг зааж өгөх

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# OpenMP санг хайх

find_package(OpenMP REQUIRED)

# Include хавтсыг зааж өгөх

include_directories(include)

# Үндсэн эх кодуудын жагсаалт

set(SRC_FILES
src/sequential.cpp
src/openmp.cpp
src/threading.cpp
src/cuda_kernel.cu
benchmark/benchmark_main.cpp
)

# Ажиллуулах файл (executable) үүсгэх

add_executable(benchmark_run ${SRC_FILES})

# OpenMP-г төсөлтэй холбох

target_link_libraries(benchmark_run PRIVATE OpenMP::OpenMP_CXX)

# CUDA салгаж компайл хийх тохиргоо

set_target_properties(benchmark_run PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
```

### 4. `Makefile` (Хялбаршуулсан команд)

Байн байн урт команд бичихгүйн тулд төслийн root хавтсанд байрлуулах `Makefile`.

# Makefile

```makefile
`.PHONY: all build run-benchmark clean

BUILD_DIR = build

all: build

build:
mkdir -p $(BUILD_DIR)
cd $(BUILD_DIR) && cmake ..
cd $(BUILD_DIR) && make

run-benchmark: build
./$(BUILD_DIR)/benchmark_run

clean:
rm -rf $(BUILD_DIR)`
```

---

**Одоо төслөө хэрхэн ажиллуулах вэ?**

Бүх файлуудаа зөв хавтсанд байрлуулсны дараа терминал дээрээ зүгээр л доорх командыг ажиллуулахад хангалттай:

# Bash

`make run-benchmark`

Энэ команд нь автоматаар `build` хавтас үүсгэж, `cmake` ажиллуулан компайл хийгээд, эцэст нь `benchmark_run` файлыг ажиллуулж дэлгэцэнд үр дүнг хэвлэх болно. Хэрэв санах ой эсвэл түр файлуудаа цэвэрлэх хэрэгтэй бол `make clean` командыг ашиглаарай.
