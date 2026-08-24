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

#include "sham/shared_memory.h"

#include <cstring>
#include <string>

#include "gtest/gtest.h"
#include "test_helpers.h"

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

static constexpr const char* kChildMessage = "Hello World!";

class SharedMemoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    name_ = sham::test::UniqueShmName("shm");
    shm_handle_ = sham::CreateFileMapping(name_, 1024);
  }

  void TearDown() override { sham::DestroyFileMapping(shm_handle_, name_); }

  std::string name_;
  sham::FileHandle shm_handle_;
};

TEST_F(SharedMemoryTest, MapAndUnmap) {
  ASSERT_NE(shm_handle_, sham::kInvalidFileHandle);

  uint8_t* ptr = sham::MapViewOfFile(shm_handle_, 1024);
  ASSERT_NE(ptr, nullptr);

  sham::UnMapViewOfFile(ptr, 1024);
}

TEST_F(SharedMemoryTest, OpenExistingAndWrite) {
  ASSERT_NE(shm_handle_, sham::kInvalidFileHandle);

  uint8_t* writer = sham::MapViewOfFile(shm_handle_, 1024);
  ASSERT_NE(writer, nullptr);
  std::strcpy(reinterpret_cast<char*>(writer), "ping");

  sham::FileHandle opened = sham::OpenFileMapping(name_);
  ASSERT_NE(opened, sham::kInvalidFileHandle);
  uint8_t* reader = sham::MapViewOfFile(opened, 1024);
  ASSERT_NE(reader, nullptr);
  EXPECT_STREQ(reinterpret_cast<char*>(reader), "ping");

  sham::UnMapViewOfFile(reader, 1024);
  sham::CloseFileMapping(opened);
  sham::UnMapViewOfFile(writer, 1024);
}

TEST(SharedMemory, OpenMissingMappingFails) {
  const std::string name = sham::test::UniqueShmName("miss");
  sham::FileHandle handle = sham::OpenFileMapping(name);
  EXPECT_EQ(handle, sham::kInvalidFileHandle);
  EXPECT_EQ(sham::MapViewOfFile(handle, 64), nullptr);
}

TEST(SharedMemory, NormalizeName) {
#ifdef _WIN32
  EXPECT_EQ(sham::NormalizeFileMappingName("abc"), "abc");
#else
  EXPECT_EQ(sham::NormalizeFileMappingName("abc"), "/abc");
  EXPECT_EQ(sham::NormalizeFileMappingName("/abc"), "/abc");
  EXPECT_EQ(sham::NormalizeFileMappingName(""), "");
#endif
}

// TODO: Support tests involving multiple processes on Windows.
#ifndef _WIN32
TEST_F(SharedMemoryTest, MultipleProcesses) {
  ASSERT_NE(shm_handle_, sham::kInvalidFileHandle);

  pid_t pid = fork();
  ASSERT_NE(pid, -1);

  if (pid == 0) {
    sham::FileHandle handle = sham::OpenFileMapping(name_);
    if (handle == sham::kInvalidFileHandle) _exit(1);
    uint8_t* ptr = sham::MapViewOfFile(handle, 1024);
    if (ptr == nullptr) {
      sham::CloseFileMapping(handle);
      _exit(2);
    }
    std::strcpy(reinterpret_cast<char*>(ptr), kChildMessage);
    sham::UnMapViewOfFile(ptr, 1024);
    sham::CloseFileMapping(handle);
    _exit(0);
  }

  int status = 0;
  ASSERT_EQ(waitpid(pid, &status, 0), pid);
  ASSERT_TRUE(WIFEXITED(status));
  ASSERT_EQ(WEXITSTATUS(status), 0);

  uint8_t* ptr = sham::MapViewOfFile(shm_handle_, 1024);
  ASSERT_NE(ptr, nullptr);
  EXPECT_STREQ(reinterpret_cast<char*>(ptr), kChildMessage);
  sham::UnMapViewOfFile(ptr, 1024);
}

TEST_F(SharedMemoryTest, AccessorCloseDoesNotUnlink) {
  ASSERT_NE(shm_handle_, sham::kInvalidFileHandle);

  sham::FileHandle accessor = sham::OpenFileMapping(name_);
  ASSERT_NE(accessor, sham::kInvalidFileHandle);
  sham::CloseFileMapping(accessor);

  sham::FileHandle still_there = sham::OpenFileMapping(name_);
  ASSERT_NE(still_there, sham::kInvalidFileHandle);
  sham::CloseFileMapping(still_there);
}
#endif
