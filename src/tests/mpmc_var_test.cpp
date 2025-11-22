#include "gtest/gtest.h"
#include "sham/queue_mpmc_var.h"
#include "sham/benchmark.h"

#include <vector>
#include <cstdint>
#include <span>

constexpr size_t kTestQueueCapacity = 128*1024;

TEST(MpmcQueueTest, PushAndPopSingleElement) {
  sham::MpmcQueue<kTestQueueCapacity> queue;

  // Data to push
  std::vector<uint8_t> data_to_push = {1, 2, 3, 4, 5};

  // Push data
  EXPECT_TRUE(queue.try_push(data_to_push));

  // Pop data
  std::vector<uint8_t> popped_data;
  EXPECT_TRUE(queue.try_pop(popped_data));

  // Verify popped data
  EXPECT_EQ(data_to_push, popped_data);

  // Queue should now be empty
  EXPECT_TRUE(queue.empty());
}

TEST(MpmcQueueTest, PushAndPopMultipleElements) {
  sham::MpmcQueue<kTestQueueCapacity> queue;

  // Push multiple elements
  for (uint8_t i = 0; i < 10; ++i) {
    std::vector<uint8_t> data_to_push = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1), static_cast<uint8_t>(i + 2)};
    EXPECT_TRUE(queue.try_push(data_to_push));
  }

  // Pop and verify elements
  for (uint8_t i = 0; i < 10; ++i) {
    std::vector<uint8_t> expected_data = {static_cast<uint8_t>(i), static_cast<uint8_t>(i + 1), static_cast<uint8_t>(i + 2)};
    std::vector<uint8_t> popped_data;
    EXPECT_TRUE(queue.try_pop(popped_data));
    EXPECT_EQ(expected_data, popped_data);
  }

  // Queue should now be empty
  EXPECT_TRUE(queue.empty());
}

TEST(MpmcQueueTest, QueueCapacityLimit) {
  sham::MpmcQueue<kTestQueueCapacity> queue;

  // Fill the queue to capacity
  std::vector<uint8_t> data_to_push(128, 42);  // 128 bytes of data
  size_t pushes = 0;

  while (queue.try_push(data_to_push)) {
    ++pushes;
  }

  // Ensure we pushed at least some elements but hit the capacity limit
  EXPECT_GT(pushes, 0);
  EXPECT_LE(pushes * (128 + sizeof(sham::MpmcQueue<kTestQueueCapacity>::Header)), kTestQueueCapacity);

  // Ensure no more elements can be pushed
  EXPECT_FALSE(queue.try_push(data_to_push));
}

TEST(MpmcQueueTest, PopFromEmptyQueue) {
  sham::MpmcQueue<kTestQueueCapacity> queue;

  // Attempt to pop from an empty queue
  std::vector<uint8_t> popped_data;
  EXPECT_FALSE(queue.try_pop(popped_data));
}

TEST(MpmcQueueTest, RandomBufferInRandomChunks_1_1)
{
  sham::BenchmarkVariableSize<sham::MpmcQueue<4096> > b{1, 1};
  b.Run();
  EXPECT_TRUE(b.GetSendBuffer() == b.GetReceiveBuffer());

  // Modify the buffer data and check that the test fails
  b.GetReceiveBuffer()[0]++;
  EXPECT_FALSE(b.GetSendBuffer() == b.GetReceiveBuffer());
}

TEST(MpmcQueueTest, RandomBufferInRandomChunks_8_1)
{
  sham::BenchmarkVariableSize<sham::MpmcQueue<16*4096> > b{8, 1};
  b.Run();
  EXPECT_TRUE(b.GetSendBuffer() == b.GetReceiveBuffer());
}

TEST(MpmcQueueTest, RandomBufferInRandomChunks_8_8)
{
  sham::BenchmarkVariableSize<sham::MpmcQueue<4096> > b{8, 8};
  b.Run();
  EXPECT_TRUE(b.GetSendBuffer() == b.GetReceiveBuffer());
}