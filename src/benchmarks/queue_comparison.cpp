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

#include <cstddef>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

#include "adapters/atomic_queue_adapter.h"
#include "adapters/concurrentqueue_adapter.h"
#include "sham/benchmark.h"
#include "sham/queue_locking.h"
#include "sham/queue_mpmc.h"

namespace {

constexpr size_t kCapacity = 64 * 1024 - 1;
constexpr size_t kNumPush = 1 * 1024 * 1024;

const std::vector<std::pair<size_t, size_t>> kThreadConfigs = {
    {1, 1}, {2, 2}, {4, 4}, {8, 1}, {1, 8},
};

template <typename QueueT>
void RunConfigs() {
  for (const auto& [push_threads, pop_threads] : kThreadConfigs) {
    sham::Benchmark<QueueT> bench(push_threads, pop_threads, kNumPush);
    bench.Run();
  }
}

}  // namespace

int main() {
  std::cout << "sham MPMC comparison: " << kNumPush << " elements, capacity " << kCapacity
            << ", hardware_concurrency=" << std::thread::hardware_concurrency() << std::endl
            << std::endl;

  std::ofstream header("benchmark_summary.txt", std::ios_base::out);
  header << "# sham MPMC comparison\n";
  header << "# elements=" << kNumPush << " capacity=" << kCapacity
         << " hardware_concurrency=" << std::thread::hardware_concurrency() << "\n";
  header.close();

  RunConfigs<sham::mpmc::LockingQueue<sham::Element, kCapacity>>();
  RunConfigs<sham::mpmc::Queue<sham::Element, kCapacity>>();
  RunConfigs<sham::AtomicQueueAdapter<sham::Element, kCapacity>>();
  RunConfigs<sham::ConcurrentQueueAdapter<sham::Element>>();

  std::cout << "Summary (push/pop million ops per second):\n";
  sham::BenchmarkStats::Get().Print();
  return 0;
}
