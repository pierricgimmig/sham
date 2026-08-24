# MIT License - Copyright (c) 2023 Pierric Gimmig

cmake_minimum_required(VERSION 3.14)
project(sham VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(SHAM_IS_TOP_LEVEL ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
else()
  set(SHAM_IS_TOP_LEVEL OFF)
endif()

option(SHAM_BUILD_TESTS "Build sham unit tests" ${SHAM_IS_TOP_LEVEL})
option(SHAM_BUILD_BENCHMARKS "Build sham benchmarks" ${SHAM_IS_TOP_LEVEL})

include(GNUInstallDirs)

# Sham library (header-only)
add_subdirectory(src/sham)

if(SHAM_BUILD_TESTS OR SHAM_BUILD_BENCHMARKS)
  find_package(concurrentqueue REQUIRED)
  find_package(atomic_queue REQUIRED)
  add_subdirectory(src/adapters)
endif()

if(SHAM_BUILD_TESTS)
  enable_testing()
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 7e33b6a1c497ced1e98fc60175aeb4678419281c
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  add_subdirectory(src/tests)
endif()

if(SHAM_BUILD_BENCHMARKS)
  include(FetchContent)
  set(BENCHMARK_ENABLE_TESTING NO)
  FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG 1c64a36c5b8ee75d462b3fe7a9d020c66a2a1094
  )
  FetchContent_MakeAvailable(googlebenchmark)
  add_subdirectory(src/benchmarks)
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/sham/include/sham"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
