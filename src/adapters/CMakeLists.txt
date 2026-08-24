# MIT License - Copyright (c) 2023 Pierric Gimmig

add_library(adapters INTERFACE)

target_sources(adapters INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/atomic_queue_adapter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/adapters/concurrentqueue_adapter.h)

target_include_directories(adapters INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(adapters INTERFACE
    atomic_queue
    concurrentqueue)
