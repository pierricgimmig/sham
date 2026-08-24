# MIT License - Copyright (c) 2023 Pierric Gimmig

include(GoogleTest)
find_package(Threads REQUIRED)

add_executable(sham_tests)

target_sources(sham_tests PRIVATE
    queue_mpmc_test.cpp
    queue_spsc_test.cpp
    shared_memory_buffer_test.cpp
    shared_memory_test.cpp)

target_link_libraries(sham_tests PRIVATE
    adapters
    sham
    GTest::gtest_main
    Threads::Threads)

target_include_directories(sham_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

gtest_discover_tests(sham_tests
    DISCOVERY_TIMEOUT 30
    PROPERTIES TIMEOUT 120)
