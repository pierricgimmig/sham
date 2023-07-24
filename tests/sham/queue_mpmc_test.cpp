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

#include <vector>

#include "gtest/gtest.h"
#include "sham/benchmark.h"

static constexpr size_t kBufferSize = 8 * 1024 * 1024;
static constexpr size_t kNumElements = 64 * 1024;

using UintQueue = sham::mpmc::Queue<uint64_t, kNumElements>;
using ElemQueue = sham::mpmc::Queue<sham::Element, 8 * 1024 * 1024>;

TEST(MpmcQueue, SameNumberOfPushAndPopAndBenchmarks8M) {
  std::vector<std::pair<size_t, size_t>> pairs = {{1, 1}, {1, 1},   {2, 2},  {4, 4},
                                                  {8, 8}, {16, 16}, {16, 1}, {32, 1}};
  for (auto [num_push_threads, num_pop_threads] : pairs) {
    sham::Benchmark<ElemQueue> b(num_push_threads, num_pop_threads);
    EXPECT_EQ(b.GetNumPushedElements(), b.GetNumPoppedElements());
  }
}
