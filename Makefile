BUILD_DIR := build
CXX := g++
NVCC := nvcc

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -pedantic -Iinclude -MMD -MP
OMPFLAGS := -fopenmp
NVCCFLAGS := -std=c++17 -O2 -Iinclude

COMMON_OBJECTS := \
	$(BUILD_DIR)/src/sequential.o \
	$(BUILD_DIR)/src/threading.o \
	$(BUILD_DIR)/src/openmp.o \
	$(BUILD_DIR)/src/cuda_kernel.o

BENCHMARK_OBJECTS := $(COMMON_OBJECTS) $(BUILD_DIR)/benchmark/benchmark_main.o
CORRECTNESS_OBJECTS := $(COMMON_OBJECTS) $(BUILD_DIR)/tests/test_correctness.o
PERFORMANCE_OBJECTS := $(COMMON_OBJECTS) $(BUILD_DIR)/tests/test_performance.o

BENCHMARK_TARGET := $(BUILD_DIR)/benchmark_run
CORRECTNESS_TARGET := $(BUILD_DIR)/test_correctness
PERFORMANCE_TARGET := $(BUILD_DIR)/test_performance

.PHONY: all build run-benchmark run-tests clean

all: build

build: $(BENCHMARK_TARGET) $(CORRECTNESS_TARGET) $(PERFORMANCE_TARGET)

run-benchmark: $(BENCHMARK_TARGET)
	./$(BENCHMARK_TARGET)

run-tests: $(CORRECTNESS_TARGET) $(PERFORMANCE_TARGET)
	./$(CORRECTNESS_TARGET) && ./$(PERFORMANCE_TARGET)

clean:
	rm -rf $(BUILD_DIR)

$(BENCHMARK_TARGET): $(BENCHMARK_OBJECTS)
	$(NVCC) $(NVCCFLAGS) $^ -o $@ -Xcompiler -fopenmp -Xcompiler -pthread

$(CORRECTNESS_TARGET): $(CORRECTNESS_OBJECTS)
	$(NVCC) $(NVCCFLAGS) $^ -o $@ -Xcompiler -fopenmp -Xcompiler -pthread

$(PERFORMANCE_TARGET): $(PERFORMANCE_OBJECTS)
	$(NVCC) $(NVCCFLAGS) $^ -o $@ -Xcompiler -fopenmp -Xcompiler -pthread

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(OMPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.cu
	@mkdir -p $(dir $@)
	$(NVCC) $(NVCCFLAGS) -c $< -o $@
