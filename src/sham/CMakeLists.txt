add_library(sham INTERFACE)

target_sources(sham INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/benchmark.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/shared_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/shared_memory_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/string_format.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/queue_mpmc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/queue_spsc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/timer.h)

target_include_directories(sham INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include)



