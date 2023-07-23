add_executable(shm)

target_sources(shm PRIVATE
    sham.cpp)

target_link_libraries(shm PRIVATE 
    sham)

add_executable(sham_tests)

target_sources(sham_tests PRIVATE
    shared_memory_test.cpp)

target_link_libraries(sham_tests PRIVATE 
    sham 
    gtest_main)
