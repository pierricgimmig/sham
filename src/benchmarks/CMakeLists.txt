# MIT License - Copyright (c) 2023 Pierric Gimmig

add_executable(sham_benchmarks)

target_sources(sham_benchmarks PRIVATE
    queue_mpmc_benchmarks.cpp)

target_link_libraries(sham_benchmarks PRIVATE
    adapters
    sham
    benchmark::benchmark
    benchmark::benchmark_main)

add_executable(sham_queue_compare)

target_sources(sham_queue_compare PRIVATE
    queue_comparison.cpp)

target_link_libraries(sham_queue_compare PRIVATE
    adapters
    sham)
