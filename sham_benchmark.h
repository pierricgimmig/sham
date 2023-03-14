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

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "sham_timer.h"

namespace sham {

struct Element {
  uint64_t thread_id;
  uint64_t timestamp_ns;
  uint64_t value;
};

template <typename QueueT>
class Benchmark {
 public:
  Benchmark(size_t num_push_threads, size_t num_pop_threads)
      : num_unregistered_threads_(num_push_threads + num_pop_threads),
        push_result_(num_push_threads),
        pop_result_(num_pop_threads) {
    queue_ = std::make_unique<QueueT>();
    std::thread push_setup_thread(&Benchmark::LaunchPushThreads, this);
    std::thread pop_setup_thread(&Benchmark::LaunchPopThreads, this);
    push_setup_thread.join();
    pop_setup_thread.join();
    Print();
  }

 private:
  // Aligned on cacheline boundary to eliminate false sharing when stored contiguously.
  struct alignas(64) ThreadResult {
    uint64_t id = 0;
    uint64_t num_operations = 0;
    uint64_t duration_ns = 0;
  };

  struct Result {
    Result(size_t size) : threads_(size), results_(size) {}
    double MillionOperationsPerSecond() {
      double seconds = static_cast<double>(time_ns_) * 0.000'000'0001;
      return (static_cast<double>(num_operations_) / seconds) * 0.000'0001;
    }
    std::vector<std::thread> threads_;
    std::vector<ThreadResult> results_;
    uint64_t time_ns_ = 0;
    uint64_t num_operations_ = 0;
  };

  void LaunchPushThreads() {
    for (size_t i = 0; i < push_result_.results_.size(); ++i) {
      push_result_.threads_[i] =
          std::thread(&Benchmark::PushThread, this, i, &push_result_.results_[i]);
    }
    BusyWaitForAllThreads();
    ScopeTimer timer(&push_result_.time_ns_);
    for (auto& thread : push_result_.threads_) {
      thread.join();
    }
  }

  void LaunchPopThreads() {
    for (size_t i = 0; i < pop_result_.results_.size(); ++i) {
      pop_result_.threads_[i] =
          std::thread(&Benchmark::PopThread, this, i, &pop_result_.results_[i]);
    }
    BusyWaitForAllThreads();
    ScopeTimer timer(&pop_result_.time_ns_);
    for (auto& thread : pop_result_.threads_) {
      thread.join();
    }
  }

  void PushThread(size_t id, ThreadResult* result) {
    result->id = id;
    size_t push_per_thread = queue_->capacity() / push_result_.threads_.size();
    RegisterAndBusyWaitForAllThreads();
    ScopeTimer timer(&result->duration_ns);
    for (size_t i = 0; i < push_per_thread; ++i) {
      if (queue_->try_push({id, id, i})) ++result->num_operations;
    }
  }

  void PopThread(size_t id, ThreadResult* result) {
    result->id = id;
    Element element;
    RegisterAndBusyWaitForAllThreads();
    ScopeTimer timer(&result->duration_ns);
    while (!queue_->empty()) {
      if (queue_->try_pop(element)) {
        ++result->num_operations;
      }
    }
  }

  void BusyWaitForAllThreads() {
    while (num_unregistered_threads_ > 0) {
    }
  }

  void RegisterAndBusyWaitForAllThreads() {
    --num_unregistered_threads_;
    BusyWaitForAllThreads();
  }

  void Print() {
    for (ThreadResult& io : push_result_.results_) {
      std::cout << "Push thread[" << io.id << "] pushed " << io.num_operations << " elements"
                << std::endl;
      push_result_.num_operations_ += io.num_operations;
    }
    std::cout << "Total pushed: " << push_result_.num_operations_ << std::endl;

    uint64_t num_popped = 0;
    for (ThreadResult& io : pop_result_.results_) {
      std::cout << "Pop thread[" << io.id << "] popped " << io.num_operations << " elements"
                << std::endl;
      pop_result_.num_operations_ += io.num_operations;
    }
    std::cout << "Total popped: " << pop_result_.num_operations_ << std::endl;
    std::cout << "Push/Pop rates " << push_result_.MillionOperationsPerSecond() << "/"
              << pop_result_.MillionOperationsPerSecond() << std::endl;
  }

 private:
  std::unique_ptr<QueueT> queue_;
  std::atomic<size_t> num_unregistered_threads_;

  Result push_result_;
  Result pop_result_;
};

}  // namespace sham