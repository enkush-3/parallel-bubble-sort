add_library(parallel_bubble_sort
	sequential.cpp
	threading.cpp
	openmp.cpp
	cuda_kernel.cu
	cuda_wrapper.cpp
)

target_include_directories(parallel_bubble_sort
	PUBLIC
		${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(parallel_bubble_sort
	PUBLIC
		OpenMP::OpenMP_CXX
		CUDA::cudart
)

set_target_properties(parallel_bubble_sort PROPERTIES
	CUDA_SEPARABLE_COMPILATION ON
)
