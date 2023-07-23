#include "sham/shared_memory.h"

#include "gtest/gtest.h"

constexpr const char* kSharedMemoryName = "shared_memory_test";

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

#ifndef _WIN32
TEST_F(SharedMemoryTest, MultipleProcesses) {
  // Fork process.
  pid_t pid = fork();
  constexpr const char* kChildMessage = "Hello World!";

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