cmake_minimum_required(VERSION 3.18)

project(ParallelBubbleSort LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)
find_package(CUDAToolkit REQUIRED)

enable_testing()

add_subdirectory(src)
add_subdirectory(benchmark)
add_subdirectory(tests)
