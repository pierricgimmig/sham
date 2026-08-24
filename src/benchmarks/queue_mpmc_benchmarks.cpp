#include <benchmark/benchmark.h>

#include "adapters/atomic_queue_adapter.h"
#include "adapters/concurrentqueue_adapter.h"
#include "sham/queue_locking.h"
#include "sham/queue_mpmc.h"

namespace {

constexpr size_t kCapacity = 1023;

using LockingInt = sham::mpmc::LockingQueue<int, kCapacity>;
using ShamMpmcInt = sham::mpmc::Queue<int, kCapacity>;
using AtomicInt = sham::AtomicQueueAdapter<int, kCapacity>;
using ConcurrentInt = sham::ConcurrentQueueAdapter<int>;

template <typename QueueT>
void BM_TryPush(benchmark::State& state) {
  QueueT queue;
  int discarded = 0;
  for (auto _ : state) {
    if (!queue.try_push(42)) {
      queue.try_pop(discarded);
      queue.try_push(42);
    }
  }
}

template <typename QueueT>
void BM_TryPop(benchmark::State& state) {
  QueueT queue;
  int value = 0;
  for (auto _ : state) {
    if (!queue.try_pop(value)) {
      queue.try_push(42);
      queue.try_pop(value);
    }
  }
}

template <typename QueueT>
void BM_TryPushThenPop(benchmark::State& state) {
  QueueT queue;
  int value = 0;
  for (auto _ : state) {
    queue.try_push(42);
    queue.try_pop(value);
  }
}

}  // namespace

BENCHMARK_TEMPLATE(BM_TryPush, LockingInt);
BENCHMARK_TEMPLATE(BM_TryPush, ShamMpmcInt);
BENCHMARK_TEMPLATE(BM_TryPush, AtomicInt);
BENCHMARK_TEMPLATE(BM_TryPush, ConcurrentInt);

BENCHMARK_TEMPLATE(BM_TryPop, LockingInt);
BENCHMARK_TEMPLATE(BM_TryPop, ShamMpmcInt);
BENCHMARK_TEMPLATE(BM_TryPop, AtomicInt);
BENCHMARK_TEMPLATE(BM_TryPop, ConcurrentInt);

BENCHMARK_TEMPLATE(BM_TryPushThenPop, LockingInt);
BENCHMARK_TEMPLATE(BM_TryPushThenPop, ShamMpmcInt);
BENCHMARK_TEMPLATE(BM_TryPushThenPop, AtomicInt);
BENCHMARK_TEMPLATE(BM_TryPushThenPop, ConcurrentInt);
