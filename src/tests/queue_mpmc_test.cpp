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

#include "sham/queue_mpmc.h"

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "adapters/atomic_queue_adapter.h"
#include "adapters/concurrentqueue_adapter.h"
#include "gtest/gtest.h"
#include "sham/benchmark.h"
#include "sham/queue_locking.h"

static constexpr size_t kQueueCapacity = 64 * 1024 - 1;
static constexpr size_t kNumPush = 64 * 1024;
static constexpr size_t kSmallNumPush = 1024;

// clang-format off

using BenchmarkQueueTypes = ::testing::Types<
  sham::mpmc::LockingQueue<sham::Element, kQueueCapacity>,
  sham::mpmc::Queue<sham::Element, kQueueCapacity>,
  sham::AtomicQueueAdapter<sham::Element, kQueueCapacity>,
  sham::ConcurrentQueueAdapter<sham::Element>>;

using SingleElementQueueTypes = ::testing::Types<
  sham::mpmc::LockingQueue<sham::Element, 1>,
  sham::mpmc::Queue<sham::Element, 1>>;

using SimpleQueueTypes = ::testing::Types<
  sham::mpmc::LockingQueue<int, 3>,
  sham::mpmc::Queue<int, 3>,
  sham::ConcurrentQueueAdapter<int>>;

template <typename T>
concept has_size_method = requires(T t) { t.size(); };

template <typename T>
concept has_empty_method = requires(T t) { t.empty(); };

template <typename T>
concept has_static_capacity = requires { T::capacity(); };

// clang-format on

#define SHAM_TYPED_TEST_SUITE(TypeName, TypeList) \
  template <typename T>                           \
  class TypeName : public ::testing::Test {};     \
  TYPED_TEST_SUITE(TypeName, TypeList);

SHAM_TYPED_TEST_SUITE(MpmcTest, BenchmarkQueueTypes);
SHAM_TYPED_TEST_SUITE(SingleElementMpmcTest, SingleElementQueueTypes);
SHAM_TYPED_TEST_SUITE(SimpleMpmcTest, SimpleQueueTypes);

template <typename QueueT>
static void RunTest(size_t num_push_threads, size_t num_pop_threads, size_t num_elements_to_push) {
  sham::Benchmark<QueueT> b(num_push_threads, num_pop_threads, num_elements_to_push);
  b.Run();

  EXPECT_EQ(b.GetNumPushedElements(), b.GetNumPoppedElements());
  EXPECT_EQ(b.GetNumPushedElements(), num_elements_to_push);

  if constexpr (has_empty_method<QueueT>) {
    EXPECT_TRUE(b.GetQueue()->empty());
  }
  if constexpr (has_size_method<QueueT>) {
    EXPECT_EQ(b.GetQueue()->size(), 0);
  }
}

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_1_1) { RunTest<TypeParam>(1, 1, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_2_2) { RunTest<TypeParam>(2, 2, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_4_4) { RunTest<TypeParam>(4, 4, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_8_1) { RunTest<TypeParam>(8, 1, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_1_8) { RunTest<TypeParam>(1, 8, kNumPush); }

TYPED_TEST(SingleElementMpmcTest, SameNumberOfPushAndPopSingleElementQueue_4_4_1K) {
  RunTest<TypeParam>(4, 4, kSmallNumPush);
}

TYPED_TEST(SimpleMpmcTest, SequentialQueueAndDequeue) {
  TypeParam q;
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_TRUE(q.try_push(3));
  if constexpr (has_static_capacity<TypeParam>) {
    EXPECT_EQ(TypeParam::capacity(), 3u);
    EXPECT_FALSE(q.try_push(4));
  }

  int value = 0;
  EXPECT_TRUE(q.try_pop(value));
  EXPECT_EQ(value, 1);
  EXPECT_TRUE(q.try_pop(value));
  EXPECT_EQ(value, 2);
  EXPECT_TRUE(q.try_pop(value));
  EXPECT_EQ(value, 3);
  EXPECT_FALSE(q.try_pop(value));

  EXPECT_TRUE(q.try_push(5));
  EXPECT_TRUE(q.try_pop(value));
  EXPECT_EQ(value, 5);
  EXPECT_FALSE(q.try_pop(value));
}

TEST(MpmcQueue, SequentialCapacityAndEmpty) {
  sham::mpmc::Queue<int, 2> q;
  EXPECT_EQ(q.capacity(), 2u);
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0);
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_FALSE(q.try_push(3));
  EXPECT_FALSE(q.empty());
  EXPECT_EQ(q.size(), 2);

  int value = 0;
  q.pop(value);
  EXPECT_EQ(value, 1);
  EXPECT_TRUE(q.try_pop(value));
  EXPECT_EQ(value, 2);
  EXPECT_FALSE(q.try_pop(value));
  EXPECT_TRUE(q.empty());
}

TEST(MpmcQueue, BlockingPushPop) {
  sham::mpmc::Queue<int, 1> q;
  q.push(9);
  int value = 0;
  q.pop(value);
  EXPECT_EQ(value, 9);
}

TEST(MpmcQueue, MultiProducerMultiConsumerPreservesValues) {
  constexpr size_t kCapacity = 1023;
  constexpr int kProducers = 4;
  constexpr int kPerProducer = 4000;
  sham::mpmc::Queue<int, kCapacity> q;
  std::atomic<int> remaining{kProducers * kPerProducer};
  std::atomic<uint64_t> sum{0};

  std::vector<std::thread> threads;
  for (int p = 0; p < kProducers; ++p) {
    threads.emplace_back([&q, p] {
      const int base = p * kPerProducer;
      for (int i = 1; i <= kPerProducer; ++i) {
        q.push(base + i);
      }
    });
  }
  for (int c = 0; c < kProducers; ++c) {
    threads.emplace_back([&q, &remaining, &sum] {
      int value = 0;
      while (remaining.load(std::memory_order_relaxed) > 0) {
        if (q.try_pop(value)) {
          sum.fetch_add(static_cast<uint64_t>(value), std::memory_order_relaxed);
          remaining.fetch_sub(1, std::memory_order_relaxed);
        }
      }
    });
  }
  for (auto& thread : threads) thread.join();

  const uint64_t expected = static_cast<uint64_t>(kProducers) * kPerProducer *
                            (static_cast<uint64_t>(kProducers) * kPerProducer + 1) / 2;
  EXPECT_EQ(sum.load(), expected);
  EXPECT_TRUE(q.empty());
}

TEST(LockingQueue, SequentialAndNonPowerOfTwoMinusOneCapacity) {
  sham::mpmc::LockingQueue<int, 4> q;
  EXPECT_EQ(q.capacity(), 4u);
  EXPECT_TRUE(q.empty());
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_TRUE(q.try_push(3));
  EXPECT_TRUE(q.try_push(4));
  EXPECT_TRUE(q.is_full());
  EXPECT_FALSE(q.try_push(5));
  EXPECT_EQ(q.size(), 4u);

  int value = 0;
  EXPECT_TRUE(q.try_pop(value));
  EXPECT_EQ(value, 1);
  EXPECT_EQ(q.size(), 3u);
  q.pop(value);
  EXPECT_EQ(value, 2);
}

TEST(LockingQueue, Description) {
  sham::mpmc::LockingQueue<int, 2> q;
  EXPECT_EQ(q.description(), "Locking queue");
  sham::mpmc::Queue<int, 2> lockfree;
  EXPECT_EQ(lockfree.description(), "Rigtorp mpmc queue");
}
