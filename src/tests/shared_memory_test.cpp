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

#include "gtest/gtest.h"

static constexpr const char* kSharedMemoryName = "shared_memory_test";
static constexpr const char* kChildMessage = "Hello World!";

class SharedMemoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create shared memory.
    shm_handle_ = sham::CreateFileMapping(kSharedMemoryName, 1024);
  }

  void TearDown() override {
    // Cleanup shared memory.
    sham::DestroyFileMapping(shm_handle_, kSharedMemoryName);
  }

  sham::FileHandle shm_handle_;
};

TEST_F(SharedMemoryTest, MapAndUnmap) {
  // Map shared memory.
  uint8_t* ptr = sham::MapViewOfFile(shm_handle_, 1024);

  // Check mapped pointer.
  ASSERT_NE(ptr, nullptr);

  // Unmap shared memory.
  sham::UnMapViewOfFile(ptr, 1024);
}

// TODO: Support tests involving multiple processes on Windows.
#ifndef _WIN32
TEST_F(SharedMemoryTest, MultipleProcesses) {
  // Fork process.
  pid_t pid = fork();

  if (pid == 0) {
    // Child process..

    // Open existing shared memory.
    sham::FileHandle handle = sham::OpenFileMapping(kSharedMemoryName);

    // Map shared memory.
    uint8_t* ptr = sham::MapViewOfFile(handle, 1024);

    // Write to shared memory.
    strcpy((char*)ptr, kChildMessage);

    // Unmap and exit.
    sham::UnMapViewOfFile(ptr, 1024);
    exit(0);
  } else {
    // Parent process.

    // Wait for child to exit.
    int status;
    waitpid(pid, &status, 0);

    // Map shared memory
    uint8_t* ptr = sham::MapViewOfFile(shm_handle_, 1024);

    // Check value written by child.
    EXPECT_STREQ((char*)ptr, kChildMessage);

    // Unmap shared memory.
    sham::UnMapViewOfFile(ptr, 1024);
  }
}
#endif