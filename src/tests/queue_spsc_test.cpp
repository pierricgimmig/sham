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

#include "sham/queue_spsc.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

TEST(SpscQueue, EmptyOnConstruct) {
  sham::SPSCQueue<int, 8> q;
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.size(), 0u);
  EXPECT_EQ(q.capacity(), 8u);
  EXPECT_EQ(q.front(), nullptr);
}

TEST(SpscQueue, TryPushTryPopSequential) {
  sham::SPSCQueue<int, 4> q;
  EXPECT_TRUE(q.try_push(1));
  EXPECT_TRUE(q.try_push(2));
  EXPECT_TRUE(q.try_push(3));
  EXPECT_TRUE(q.try_push(4));
  EXPECT_FALSE(q.try_push(5));
  EXPECT_EQ(q.size(), 4u);
  EXPECT_FALSE(q.empty());

  ASSERT_NE(q.front(), nullptr);
  EXPECT_EQ(*q.front(), 1);
  q.pop();
  ASSERT_NE(q.front(), nullptr);
  EXPECT_EQ(*q.front(), 2);
  q.pop();
  ASSERT_NE(q.front(), nullptr);
  EXPECT_EQ(*q.front(), 3);
  q.pop();
  ASSERT_NE(q.front(), nullptr);
  EXPECT_EQ(*q.front(), 4);
  q.pop();

  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.front(), nullptr);
  EXPECT_EQ(q.size(), 0u);
}

TEST(SpscQueue, WrapAround) {
  sham::SPSCQueue<int, 2> q;
  EXPECT_TRUE(q.try_push(10));
  EXPECT_TRUE(q.try_push(20));
  EXPECT_FALSE(q.try_push(30));

  EXPECT_EQ(*q.front(), 10);
  q.pop();
  EXPECT_TRUE(q.try_push(30));
  EXPECT_EQ(*q.front(), 20);
  q.pop();
  EXPECT_EQ(*q.front(), 30);
  q.pop();
  EXPECT_TRUE(q.empty());
}

TEST(SpscQueue, EmplaceAndPush) {
  sham::SPSCQueue<int, 3> q;
  q.emplace(7);
  q.push(8);
  EXPECT_EQ(q.size(), 2u);
  EXPECT_EQ(*q.front(), 7);
  q.pop();
  EXPECT_EQ(*q.front(), 8);
  q.pop();
}

TEST(SpscQueue, MoveOnlyType) {
  sham::SPSCQueue<std::unique_ptr<int>, 3> q;
  EXPECT_TRUE(q.try_push(std::make_unique<int>(11)));
  EXPECT_TRUE(q.try_emplace(std::make_unique<int>(22)));
  EXPECT_EQ(**q.front(), 11);
  q.pop();
  EXPECT_EQ(**q.front(), 22);
  q.pop();
  EXPECT_TRUE(q.empty());
}

TEST(SpscQueue, DestructorDestroysRemainingElements) {
  std::atomic<int> live{0};
  struct Counted {
    std::atomic<int>* live;
    explicit Counted(std::atomic<int>* l) : live(l) { live->fetch_add(1); }
    Counted(const Counted&) = delete;
    Counted(Counted&& other) noexcept : live(other.live) { other.live = nullptr; }
    ~Counted() {
      if (live) live->fetch_sub(1);
    }
  };

  {
    sham::SPSCQueue<Counted, 4> q;
    EXPECT_TRUE(q.try_emplace(&live));
    EXPECT_TRUE(q.try_emplace(&live));
    EXPECT_EQ(live.load(), 2);
  }
  EXPECT_EQ(live.load(), 0);
}

TEST(SpscQueue, SingleProducerSingleConsumer) {
  constexpr size_t kCount = 50'000;
  sham::SPSCQueue<uint64_t, 1024> q;
  std::atomic<bool> producer_done{false};
  uint64_t checksum = 0;

  std::thread producer([&] {
    for (uint64_t i = 0; i < kCount; ++i) {
      while (!q.try_push(i)) {
      }
    }
    producer_done.store(true, std::memory_order_release);
  });

  std::thread consumer([&] {
    uint64_t expected = 0;
    while (expected < kCount) {
      uint64_t* value = q.front();
      if (value == nullptr) continue;
      EXPECT_EQ(*value, expected);
      checksum += *value;
      q.pop();
      ++expected;
    }
  });

  producer.join();
  consumer.join();
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(checksum, (kCount - 1) * kCount / 2);
}
