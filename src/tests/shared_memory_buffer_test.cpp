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

#include <cstdint>
#include <memory>

#include "gtest/gtest.h"
#include "sham/queue_mpmc.h"
#include "test_helpers.h"

TEST(SharedMemoryBufferTest, CreateAndAccess) {
  const std::string name = sham::test::UniqueShmName("buf");
  auto buffer = std::make_unique<sham::SharedMemoryBuffer>(name, 1024,
                                                           sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buffer->valid());
  EXPECT_EQ(buffer->capacity(), 1024u);
  EXPECT_EQ(buffer->type(), sham::SharedMemoryBuffer::Type::kCreate);

  int* ptr = buffer->Allocate<int>();
  ASSERT_NE(ptr, nullptr);
  *ptr = 42;
  EXPECT_EQ(*buffer->As<int>(), 42);
}

TEST(SharedMemoryBufferTest, MultipleAccess) {
  const std::string name = sham::test::UniqueShmName("macc");
  auto buffer = std::make_unique<sham::SharedMemoryBuffer>(name, 1024,
                                                           sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buffer->valid());

  sham::SharedMemoryBuffer buffer2(name, 1024, sham::SharedMemoryBuffer::Type::kAccessExisting);
  ASSERT_TRUE(buffer2.valid());
  EXPECT_EQ(buffer2.type(), sham::SharedMemoryBuffer::Type::kAccessExisting);

  int* ptr1 = buffer->Allocate<int>();
  ASSERT_NE(ptr1, nullptr);
  *ptr1 = 123;

  int* ptr2 = buffer2.As<int>();
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(*ptr2, 123);
}

TEST(SharedMemoryBufferTest, AllocateTooMuch) {
  const std::string name = sham::test::UniqueShmName("full");
  auto buffer = std::make_unique<sham::SharedMemoryBuffer>(name, sizeof(int),
                                                           sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buffer->valid());

  int* ptr = buffer->Allocate<int>(42);
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(*ptr, 42);

  int* ptr2 = buffer->Allocate<int>(42);
  EXPECT_EQ(ptr2, nullptr);
}

TEST(SharedMemoryBufferTest, AllocateMultiple) {
  const std::string name = sham::test::UniqueShmName("many");
  auto buffer = std::make_unique<sham::SharedMemoryBuffer>(name, 1024,
                                                           sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buffer->valid());

  int* ptr1 = buffer->Allocate<int>(42);
  int* ptr2 = buffer->Allocate<int>(43);
  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(*ptr1, 42);
  EXPECT_EQ(*ptr2, 43);
  EXPECT_GE(buffer->size(), 2 * sizeof(int));
}

TEST(SharedMemoryBufferTest, AllocateRespectsAlignment) {
  const std::string name = sham::test::UniqueShmName("align");
  sham::SharedMemoryBuffer buffer(name, 1024, sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buffer.valid());

  uint8_t* byte = buffer.Allocate(1, 1);
  ASSERT_NE(byte, nullptr);

  struct alignas(64) Aligned {
    uint64_t value;
  };
  Aligned* aligned = buffer.Allocate<Aligned>();
  ASSERT_NE(aligned, nullptr);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(aligned) % 64u, 0u);
  aligned->value = 99;
  EXPECT_EQ(aligned->value, 99u);
}

TEST(SharedMemoryBufferTest, MoveConstructor) {
  const std::string name = sham::test::UniqueShmName("mv");
  sham::SharedMemoryBuffer buf1(name, 1024, sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buf1.valid());

  sham::SharedMemoryBuffer buf2(std::move(buf1));
  EXPECT_FALSE(buf1.valid());
  EXPECT_EQ(buf1.type(), sham::SharedMemoryBuffer::Type::kInvalid);
  ASSERT_TRUE(buf2.valid());
  EXPECT_EQ(buf2.capacity(), 1024u);
}

TEST(SharedMemoryBufferTest, MoveAssignment) {
  const std::string name1 = sham::test::UniqueShmName("mva");
  const std::string name2 = sham::test::UniqueShmName("mvb");
  sham::SharedMemoryBuffer buf1(name1, 1024, sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buf1.valid());

  sham::SharedMemoryBuffer buf2(name2, 512, sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(buf2.valid());

  buf2 = std::move(buf1);
  EXPECT_FALSE(buf1.valid());
  ASSERT_TRUE(buf2.valid());
  EXPECT_EQ(buf2.capacity(), 1024u);
}

TEST(SharedMemoryBufferTest, FailedOpenIsInvalid) {
  const std::string name = sham::test::UniqueShmName("nope");
  sham::SharedMemoryBuffer buffer(name, 128, sham::SharedMemoryBuffer::Type::kAccessExisting);
  EXPECT_FALSE(buffer.valid());
  EXPECT_EQ(buffer.Allocate(8), nullptr);
  EXPECT_EQ(buffer.As<int>(), nullptr);
}

#ifndef _WIN32
TEST(SharedMemoryBufferTest, AccessorDestructorDoesNotUnlink) {
  const std::string name = sham::test::UniqueShmName("own");
  auto creator = std::make_unique<sham::SharedMemoryBuffer>(
      name, 256, sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(creator->valid());
  int* value = creator->Allocate<int>(7);
  ASSERT_NE(value, nullptr);

  {
    sham::SharedMemoryBuffer accessor(name, 256, sham::SharedMemoryBuffer::Type::kAccessExisting);
    ASSERT_TRUE(accessor.valid());
    EXPECT_EQ(*accessor.As<int>(), 7);
  }

  sham::SharedMemoryBuffer still_there(name, 256, sham::SharedMemoryBuffer::Type::kAccessExisting);
  ASSERT_TRUE(still_there.valid());
  EXPECT_EQ(*still_there.As<int>(), 7);
}

TEST(SharedMemoryBufferTest, PlaceMpmcQueueInSharedMemory) {
  using Queue = sham::mpmc::Queue<int, 7>;
  const std::string name = sham::test::UniqueShmName("q");
  sham::SharedMemoryBuffer creator(name, sizeof(Queue) + 64,
                                   sham::SharedMemoryBuffer::Type::kCreate);
  ASSERT_TRUE(creator.valid());

  Queue* queue = creator.Allocate<Queue>();
  ASSERT_NE(queue, nullptr);
  EXPECT_TRUE(queue->try_push(11));
  EXPECT_TRUE(queue->try_push(22));

  sham::SharedMemoryBuffer accessor(name, sizeof(Queue) + 64,
                                    sham::SharedMemoryBuffer::Type::kAccessExisting);
  ASSERT_TRUE(accessor.valid());
  Queue* remote =
      accessor.As<Queue>(static_cast<size_t>(reinterpret_cast<uint8_t*>(queue) - creator.data()));
  ASSERT_NE(remote, nullptr);

  int value = 0;
  EXPECT_TRUE(remote->try_pop(value));
  EXPECT_EQ(value, 11);
  EXPECT_TRUE(remote->try_pop(value));
  EXPECT_EQ(value, 22);
}
#endif
