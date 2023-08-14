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

TEST(SharedMemoryBufferTest, CreateAndAccess) {
  auto buffer_ = std::make_unique<sham::SharedMemoryBuffer>(
      kSharedMemoryName, 1024, sham::SharedMemoryBuffer::Type::kCreate);
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

TEST(SharedMemoryBufferTest, MultipleAccess) {
  auto buffer_ = std::make_unique<sham::SharedMemoryBuffer>(
      kSharedMemoryName, 1024, sham::SharedMemoryBuffer::Type::kCreate);
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

TEST(SharedMemoryBuffer, AllocateTooMuch) {
  // Allocate memory
  auto buffer = std::make_unique<sham::SharedMemoryBuffer>(kSharedMemoryName, sizeof(int),
                                                           sham::SharedMemoryBuffer::Type::kCreate);
  int* ptr = buffer->Allocate<int>(42);

  // Check memory was allocated
  ASSERT_NE(ptr, nullptr);

  // Allocate too much memory
  int* ptr2 = buffer->Allocate<int>(42);

  // Check memory was not allocated
  EXPECT_EQ(ptr2, nullptr);
}

TEST(SharedMemoryBufferTest, AllocateMultiple) {
  auto buffer_ = std::make_unique<sham::SharedMemoryBuffer>(
      kSharedMemoryName, 1024, sham::SharedMemoryBuffer::Type::kCreate);
  // Allocate memory
  int* ptr1 = buffer_->Allocate<int>(42);
  int* ptr2 = buffer_->Allocate<int>(43);

  // Check memory was allocated
  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);

  // Check values
  EXPECT_EQ(*ptr1, 42);
  EXPECT_EQ(*ptr2, 43);
}

TEST(SharedMemoryBuffer, MoveConstructor) {
  sham::SharedMemoryBuffer buf1("test", 1024, sham::SharedMemoryBuffer::Type::kCreate);

  ASSERT_TRUE(buf1.valid());

  sham::SharedMemoryBuffer buf2(std::move(buf1));

  ASSERT_FALSE(buf1.valid());
  ASSERT_TRUE(buf2.valid());

  ASSERT_EQ(buf2.capacity(), 1024);
}

TEST(SharedMemoryBuffer, MoveAssignment) {
  sham::SharedMemoryBuffer buf1("test", 1024, sham::SharedMemoryBuffer::Type::kCreate);

  ASSERT_TRUE(buf1.valid());

  sham::SharedMemoryBuffer buf2("other", 512, sham::SharedMemoryBuffer::Type::kCreate);

  ASSERT_TRUE(buf2.valid());

  buf2 = std::move(buf1);

  ASSERT_FALSE(buf1.valid());
  ASSERT_TRUE(buf2.valid());

  ASSERT_EQ(buf2.capacity(), 1024);
}
