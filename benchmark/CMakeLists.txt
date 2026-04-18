add_executable(benchmark_run benchmark_main.cpp)

target_link_libraries(benchmark_run
	PRIVATE
		parallel_bubble_sort
)
