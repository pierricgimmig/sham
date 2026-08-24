# MIT License - Copyright (c) 2023 Pierric Gimmig

add_library(sham INTERFACE)

target_sources(sham INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/benchmark.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/shared_memory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/shared_memory_buffer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/string_format.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/queue_mpmc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/queue_locking.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/queue_spsc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sham/timer.h)

target_include_directories(sham INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

target_compile_features(sham INTERFACE cxx_std_20)

if(UNIX AND NOT APPLE)
  find_library(SHAM_LIBRT rt)
  if(SHAM_LIBRT)
    target_link_libraries(sham INTERFACE ${SHAM_LIBRT})
  endif()
endif()
