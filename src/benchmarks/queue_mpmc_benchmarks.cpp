#include <benchmark/benchmark.h>

#include "adapters/atomic_queue_adapter.h"
#include "adapters/concurrentqueue_adapter.h"

using LocklessQueue = sham::ConcurrentQueueAdapter<int>;

// Benchmark for push operation
static void BM_LocklessQueuePush(benchmark::State& state) {
  LocklessQueue queue;
  for (auto _ : state) {
    queue.push(42);
  }
}
BENCHMARK(BM_LocklessQueuePush);

// Benchmark for pop operation
static void BM_LocklessQueuePop(benchmark::State& state) {
  LocklessQueue queue;
  queue.push(42);  // Insert an element to ensure pop has something to do
  for (auto _ : state) {
    int value;
    queue.try_pop(value);
    queue.push(42);  // Re-insert to maintain steady state
  }
}
BENCHMARK(BM_LocklessQueuePop);