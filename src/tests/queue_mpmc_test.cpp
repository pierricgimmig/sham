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
#include "sham/queue_mpmc_var.h"

#include "adapters/atomic_queue_adapter.h"
#include "adapters/concurrentqueue_adapter.h"
#include "adapters/mpmc_var_adapter.h"
#include "gtest/gtest.h"
#include "sham/benchmark.h"
#include "sham/queue_locking.h"

// static constexpr size_t kQueueCapacity = 1 * 1024 * 1024 - 1;
static constexpr size_t kQueueCapacity = 128-1;
static constexpr size_t kNumPush = 8 * 1024 * 1024;
static constexpr size_t kSmallNumPush = 1024;

// clang-format off

using BenchmarkQueueTypes = ::testing::Types<
  //sham::mpmc::LockingQueue<sham::Element, kQueueCapacity>,
  //sham::mpmc::Queue<sham::Element, kQueueCapacity>,
  //sham::AtomicQueueAdapter<sham::Element, kQueueCapacity>,
  //sham::ConcurrentQueueAdapter<sham::Element>,
  sham::MpmcVarQueueAdapter<sham::Element, kQueueCapacity>>;

using SingleEmlementQueueTypes = ::testing::Types<
  sham::mpmc::LockingQueue<sham::Element, 1>,
  sham::mpmc::Queue<sham::Element, 1>>;

using SimpleQueueTypes = ::testing::Types<
  sham::mpmc::LockingQueue<int, 3>, 
  sham::mpmc::Queue<int, 3>,
  sham::ConcurrentQueueAdapter<int>,
  sham::MpmcVarQueueAdapter<int, 3>>;

template <typename T>
concept has_size_method = requires(T t) { t.size(); };

template <typename T>
concept has_empty_method = requires(T t) { t.empty(); };

// clang-format on

#define SHAM_TYPED_TEST_SUITE(TypeName, TypeList) \
  template <typename T>                           \
  class TypeName : public ::testing::Test {};     \
  TYPED_TEST_SUITE(TypeName, TypeList);

SHAM_TYPED_TEST_SUITE(MpmcTest, BenchmarkQueueTypes);
SHAM_TYPED_TEST_SUITE(SingleElementMpmcTest, SingleEmlementQueueTypes);
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

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_1_1_8M) { RunTest<TypeParam>(1, 1, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_2_1_8M) { RunTest<TypeParam>(2, 1, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_2_2_8M) { RunTest<TypeParam>(2, 2, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_4_4_8M) { RunTest<TypeParam>(4, 4, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_8_8_8M) { RunTest<TypeParam>(8, 8, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_16_16_8M) { RunTest<TypeParam>(16, 16, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_16_1_8M) { RunTest<TypeParam>(16, 1, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_32_1_8M) { RunTest<TypeParam>(32, 1, kNumPush); }

TYPED_TEST(MpmcTest, SameNumberOfPushAndPop_1_16_8M) { RunTest<TypeParam>(1, 16, kNumPush); }

TYPED_TEST(SingleElementMpmcTest, SameNumberOfPushAndPopSingleElementQueue_4_4_1K) {
  RunTest<TypeParam>(4, 4, kSmallNumPush);
}

TYPED_TEST(SimpleMpmcTest, SequentialQueueAndDequeue) {
  sham::mpmc::LockingQueue<int, 3> q;
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_TRUE(q.try_push(3));
  EXPECT_FALSE(q.try_push(4));

  int value;
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