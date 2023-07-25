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

#include "gtest/gtest.h"
#include "sham/benchmark.h"

namespace {
constexpr size_t kQueueCapacity = 1 * 1024 * 1024;
constexpr size_t kNumElements = 8 * 1024 * 1024;
using ElemQueue = sham::mpmc::Queue<sham::Element, kQueueCapacity>;

template <typename QueueT>
void RunTest(size_t num_push_threads, size_t num_pop_threads, size_t num_elements_to_push) {
  sham::Benchmark<QueueT> b(num_push_threads, num_pop_threads, num_elements_to_push);
  b.Run();
  EXPECT_EQ(b.GetNumPushedElements(), b.GetNumPoppedElements());
  EXPECT_EQ(b.GetNumPushedElements(), num_elements_to_push);
  EXPECT_TRUE(b.GetQueue()->empty());
}
}  // namespace

TEST(MpmcQueue, SameNumberOfPushAndPopSingleElementQueue_4_4_1K) {
  using SingleElemQueue = sham::mpmc::Queue<sham::Element, /*capacity=*/1>;
  RunTest<SingleElemQueue>(4, 4, /*num_elements_to_push*/ 1024);
}

TEST(MpmcQueue, SameNumberOfPushAndPop_1_1_8M) { RunTest<ElemQueue>(1, 1, kNumElements); }

TEST(MpmcQueue, SameNumberOfPushAndPop_2_2_8M) { RunTest<ElemQueue>(2, 2, kNumElements); }

TEST(MpmcQueue, SameNumberOfPushAndPop_4_4_8M) { RunTest<ElemQueue>(4, 4, kNumElements); }

TEST(MpmcQueue, SameNumberOfPushAndPop_8_8_8M) { RunTest<ElemQueue>(8, 8, kNumElements); }

TEST(MpmcQueue, SameNumberOfPushAndPop_16_16_8M) { RunTest<ElemQueue>(16, 16, kNumElements); }

TEST(MpmcQueue, SameNumberOfPushAndPop_16_1_8M) { RunTest<ElemQueue>(16, 1, kNumElements); }

TEST(MpmcQueue, SameNumberOfPushAndPop_32_1_8M) { RunTest<ElemQueue>(32, 1, kNumElements); }

TEST(MpmcQueue, SameNumberOfPushAndPop_1_16_8M) { RunTest<ElemQueue>(1, 16, kNumElements); }