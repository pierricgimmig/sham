#include "sham/shared_memory_buffer.h"

#include "gtest/gtest.h"

static constexpr const char* kSharedMemoryName = "shared_memory_buffer_test";

class SharedMemoryBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    buffer_ = std::make_unique<sham::SharedMemoryBuffer>(kSharedMemoryName, 1024,
                                                         sham::SharedMemoryBuffer::Type::kCreate);
  }

  void TearDown() override { buffer_.reset(); }

  std::unique_ptr<sham::SharedMemoryBuffer> buffer_;
};

TEST_F(SharedMemoryBufferTest, CreateAndAccess) {
  // Check capacity
  EXPECT_EQ(buffer_->capacity(), 1024);

  // Allocate memory
  int* ptr = buffer_->Allocate<int>();

  // Check memory was allocated
  ASSERT_NE(ptr, nullptr);

  // Write value
  *ptr = 42;

  // Read back
  EXPECT_EQ(*buffer_->As<int>(), 42);
}

TEST_F(SharedMemoryBufferTest, MultipleAccess) {
  // Create second buffer for same shared memory
  sham::SharedMemoryBuffer buffer2(kSharedMemoryName, 1024,
                                   sham::SharedMemoryBuffer::Type::kAccessExisting);

  // Allocate in first buffer
  int* ptr1 = buffer_->Allocate<int>();
  *ptr1 = 123;

  // Read value from second buffer
  int* ptr2 = buffer2.As<int>();
  EXPECT_EQ(*ptr2, 123);
}
