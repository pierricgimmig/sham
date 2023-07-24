# MIT License - Copyright (c) 2023 Pierric Gimmig

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

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



