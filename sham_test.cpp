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
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "sham_buffer.h"
#include "sham_queue_mpmc.h"
#include "sham_queue_spsc.h"
#include "sham_timer.h"

#define TRACE_VAR(x) std::cout << #x << " = " << x << std::endl

constexpr size_t kBufferSize = 8 * 1024 * 1024;
constexpr size_t kNumElements = 64 * 1024;

using namespace sham;

double OperationsPerSecond(uint64_t num_operations, uint64_t duration_ns) {
  double seconds = static_cast<double>(duration_ns) * 0.000'000'0001;
  return static_cast<double>(num_operations) / seconds;
}
double MillionOperationsPerSecond(uint64_t num_operations, uint64_t duration_ns) {
  return OperationsPerSecond(num_operations, duration_ns) * 0.000'0001;
}

struct Element {
  uint64_t thread_id;
  uint64_t timestamp_ns;
  uint64_t value;
};

// Stores result of benchmark. Aligned on cacheline boundary to eliminate false sharing.
struct alignas(64) BenchmarkIO {
  uint64_t id = 0;
  uint64_t num_operations = 0;
  uint64_t duration_ns = 0;
};


template <typename QueueT>
class Benchmark {
 public:
  Benchmark(size_t num_push_threads, size_t num_pop_threads) {
    num_unregistered_threads_ = num_push_threads + num_pop_threads;
    std::thread push_thread(&Benchmark::CreatePushThreads, this, num_push_threads);
    std::thread pop_thread(&Benchmark::CreatePopThreads, this, num_pop_threads);
    push_thread.join();
    pop_thread.join();

    for(BenchmarkIO& io : push_results) {
      std::cout << "Push thread[" << io.id << "] pushed" << io.num_operations << " elements" << std::endl;
      num_pushed_elements_ += io.num_operations;
    }
    std::cout << "Total pushed: " << num_pushed_elements_ << std::endl;

    uint64_t num_popped = 0;
    for(BenchmarkIO& io : pop_results) {
      std::cout << "Pop thread[" << io.id << "] popped" << io.num_operations << " elements" << std::endl;
      num_popped_elements_ += io.num_operations;
    }
    std::cout << "Total popped: " << num_popped_elements_ << std::endl;

    double push_rate = MillionOperationsPerSecond(num_pushed_elements_, push_time_ns_);
    double pop_rate = MillionOperationsPerSecond(num_popped_elements_, pop_time_ns_);
    TRACE_VAR(push_rate);
    TRACE_VAR(pop_rate);
  }

  std::string GetSummary() { return ""; }

 private:
  //void CreateThreads(size_t num_threads, std::function<void()> function)
  void CreatePushThreads(size_t num_threads) {
    push_threads.resize(num_threads);
    push_results.resize(num_threads);
    size_t num_push_per_thread = queue.capacity() / num_threads;
    for (size_t i = 0; i < num_threads; ++i) {
      BenchmarkIO& io = push_results[i];
      io.id = i;
      push_threads[i] = std::thread(&Benchmark::PushThread, this, &queue, num_push_per_thread, &io);
    }

    BusyWaitForAllThreads();
    ScopeTimer timer(&push_time_ns_);
    for (auto& thread : push_threads) {
      thread.join();
    }
  }

  void CreatePopThreads(size_t num_threads) {
    pop_threads.resize(num_threads);
    pop_results.resize(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
      BenchmarkIO& io = pop_results[i];
      io.id = i;
      pop_threads[i] = std::thread(&Benchmark::PopThread, this, &queue, &io);
    }

    BusyWaitForAllThreads();
    ScopeTimer timer(&pop_time_ns_);
    for (auto& thread : pop_threads) {
      thread.join();
    }
  }

  void PushThread(QueueT* queue, size_t num_pushes, BenchmarkIO* io) {
    RegisterAndBusyWaitForAllThreads();
    ScopeTimer timer(&io->duration_ns);
    for (size_t i = 0; i < num_pushes; ++i) {
      if(queue->try_push({io->id, io->id, i}))
        ++io->num_operations;
    }
  }

  void PopThread(QueueT* queue, BenchmarkIO* io) {
    RegisterAndBusyWaitForAllThreads();
    ScopeTimer timer(&io->duration_ns);
    Element element;
    while (!queue->empty()) {
      if (queue->try_pop(element)) {
        ++io->num_operations;
      }
    }
  }

  void RegisterAndBusyWaitForAllThreads() {
    --num_unregistered_threads_;
    BusyWaitForAllThreads();
  }

  void BusyWaitForAllThreads() {
    while(num_unregistered_threads_ > 0) {}
  }

 private:
  QueueT queue;

  std::atomic<size_t> num_unregistered_threads_;

  std::vector<std::thread> push_threads;
  std::vector<BenchmarkIO> push_results;
  uint64_t push_time_ns_ = 0;
  uint64_t num_pushed_elements_ = 0;

  std::vector<std::thread> pop_threads;
  std::vector<BenchmarkIO> pop_results;
  uint64_t pop_time_ns_ = 0;
  uint64_t num_popped_elements_ = 0;
};

// using UintQueue = sham::SPSCQueue<uint32_t, kNumElements>;
using UintQueue = sham::mpmc::Queue<uint64_t, kNumElements>;
using ElemQueue = sham::mpmc::Queue<Element, 8 * 1024*1024>;

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
  std::vector<std::pair<size_t, size_t>> pairs = {{1, 1}, {1, 1}, {2, 2}, {4, 4},
                                                  {8, 8}, {16, 16}, {16, 1}, {32, 1}};
  for (auto [num_push_threads, num_pop_threads] : pairs) {
    auto b = std::make_unique<Benchmark<ElemQueue>>(num_push_threads, num_pop_threads);
  }

  std::cout << "hello, sham!\n";
  if (argc == 1)
    create_sham();
  else
    read_sham();
}
