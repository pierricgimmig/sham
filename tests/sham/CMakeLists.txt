include(GoogleTest)
add_executable(shm)

target_sources(shm PRIVATE
    sham.cpp)

target_link_libraries(shm PRIVATE 
    sham)

add_executable(sham_tests)

target_sources(sham_tests PRIVATE
    shared_memory_test.cpp
    shared_memory_buffer_test.cpp)

target_link_libraries(sham_tests PRIVATE 
    sham 
    gtest_main)

gtest_discover_tests(sham_tests)
