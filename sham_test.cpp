/*
MIT License - Copyright (c) 2023 Pierric Gimmig

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "sham_benchmark.h"
#include "sham_buffer.h"
#include "sham_queue_mpmc.h"
#include "sham_queue_spsc.h"
#include "sham_timer.h"

#define TRACE_VAR(x) std::cout << #x << " = " << x << std::endl

constexpr size_t kBufferSize = 8 * 1024 * 1024;
constexpr size_t kNumElements = 64 * 1024;

using namespace sham;

// using UintQueue = sham::SPSCQueue<uint32_t, kNumElements>;
using UintQueue = sham::mpmc::Queue<uint64_t, kNumElements>;
using ElemQueue = sham::mpmc::Queue<Element, 8 * 1024 * 1024>;

int create_sham() {
  sham::SharedMemoryBuffer shared_memory_buffer("/my_memory", kBufferSize);

  TRACE_VAR(shared_memory_buffer.size());
  UintQueue* q = shared_memory_buffer.Allocate<UintQueue>();
  TRACE_VAR(shared_memory_buffer.size());
  TRACE_VAR(sizeof(UintQueue));
  TRACE_VAR(shared_memory_buffer.size());
  TRACE_VAR(shared_memory_buffer.size() - sizeof(UintQueue));
  TRACE_VAR(shared_memory_buffer.capacity());
  TRACE_VAR(float(sizeof(*q)) / float(kNumElements));

  uint32_t i = 0;
  while (q->size() != q->capacity()) q->push(++i);

  std::cout << "Queue has: " << q->size() << " elements" << std::endl;

  while (!q->empty()) {
  }

  std::cout << "Queue is empty!" << std::endl;
  return 0;
}

int read_sham() {
  sham::SharedMemoryBufferView shared_memory_buffer_view("/my_memory", kBufferSize);
  UintQueue* q = shared_memory_buffer_view.As<UintQueue>();

  TRACE_VAR(q->size());
  TRACE_VAR(q->capacity());
  TRACE_VAR(q->empty());
  uint64_t i;
  while (!q->empty()) {
    q->pop(i);
  }
  TRACE_VAR(i);

  std::cout << "Queue is empty!" << std::endl;
  return 0;
}

int read_sham_in_loop() {
  while (true) {
    sham::SharedMemoryBufferView shared_memory_buffer_view("/my_memory", kBufferSize);
    UintQueue* q = shared_memory_buffer_view.As<UintQueue>();
    TRACE_VAR(q->size());
    TRACE_VAR(q->capacity());
    TRACE_VAR(q->empty());
    size_t num_pops = 100;
    uint64_t i;
    while (--num_pops > 0) {
      q->pop(i);
    }
    TRACE_VAR(i);
  }

  return 0;
}

int main(int argc, char** argv) {
  std::vector<std::pair<size_t, size_t>> pairs = {{1, 1}, {1, 1},   {2, 2},  {4, 4},
                                                  {8, 8}, {16, 16}, {16, 1}, {32, 1}};
  for (auto [num_push_threads, num_pop_threads] : pairs) {
    Benchmark<ElemQueue> b(num_push_threads, num_pop_threads);
  }

  std::cout << "hello, sham!\n";
  if (argc == 1)
    create_sham();
  else
    read_sham();
}
