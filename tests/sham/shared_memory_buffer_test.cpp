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
