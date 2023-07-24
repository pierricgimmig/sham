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

static constexpr size_t kQueueCapacity = 1 * 1024 * 1024;
static constexpr size_t kNumElements = 8 * 1024 * 1024;
using ElemQueue = sham::mpmc::Queue<sham::Element, kQueueCapacity>;

TEST(MpmcQueue, SameNumberOfPushAndPopSingleElementQueue_1K_4_4) {
  constexpr size_t kSingleElementCapacity = 1;
  constexpr size_t kSmallNumElements = 1024;
  using SmallElemQueue = sham::mpmc::Queue<sham::Element, kSingleElementCapacity>;
  sham::Benchmark<SmallElemQueue> b(/*num_push_threads=*/4, /*num_pop_threads=*/4,
                                    kSmallNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kSmallNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kSmallNumElements);
}

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks_8M_1_1) {
  sham::Benchmark<ElemQueue> b(/*num_push_threads=*/1, /*num_pop_threads=*/1, kNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kNumElements);
}

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks_8M_2_2) {
  sham::Benchmark<ElemQueue> b(/*num_push_threads=*/2, /*num_pop_threads=*/2, kNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kNumElements);
}

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks_8M_4_4) {
  sham::Benchmark<ElemQueue> b(/*num_push_threads=*/4, /*num_pop_threads=*/4, kNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kNumElements);
}

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks_8M_8_8) {
  sham::Benchmark<ElemQueue> b(/*num_push_threads=*/8, /*num_pop_threads=*/8, kNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kNumElements);
}

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks_8M_16_16) {
  sham::Benchmark<ElemQueue> b(/*num_push_threads=*/16, /*num_pop_threads=*/16, kNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kNumElements);
}

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks_8M_16_1) {
  sham::Benchmark<ElemQueue> b(/*num_push_threads=*/16, /*num_pop_threads=*/1, kNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kNumElements);
}

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks_8M_32_1) {
  sham::Benchmark<ElemQueue> b(/*num_push_threads=*/32, /*num_pop_threads=*/1, kNumElements);
  EXPECT_EQ(b.GetNumPushedElements(), kNumElements);
  EXPECT_EQ(b.GetNumPoppedElements(), kNumElements);
}
