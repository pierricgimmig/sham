#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "SPSCQueue.h"
#include "ShamBuffer.h"
#include "ShamQueue.h"
#include "ShamQueueMPMC.h"

#define TRACE_VAR(x) std::cout << #x << " = " << x << std::endl

constexpr size_t kBufferSize = 8 * 1024 * 1024;
constexpr size_t kNumElements = 64 * 1024;

// using UintQueue = sham::SPSCQueue<uint32_t, kNumElements>;
using UintQueue = sham::mpmc::Queue<uint64_t, kNumElements>;

int create_sham() {
  sham::SharedMemoryBuffer shared_memory_buffer("/my_memory", kBufferSize,
                                                sham::SharedMemoryBuffer::Type::kCreate);

  TRACE_VAR(shared_memory_buffer.size());
  UintQueue* q = shared_memory_buffer.Allocate<UintQueue>();
  TRACE_VAR(shared_memory_buffer.size());
  TRACE_VAR(sizeof(UintQueue));
  TRACE_VAR(shared_memory_buffer.size());
  TRACE_VAR(shared_memory_buffer.size() - sizeof(UintQueue));
  TRACE_VAR(shared_memory_buffer.capacity());
  TRACE_VAR(float(sizeof(*q)) / float(kNumElements));

  uint32_t i = 0;
  while (q->size() != q->capacity()) q->push(++i);

  std::cout << "Queue has: " << q->size() << " elements" << std::endl;

  while (!q->empty()) {
  }

  std::cout << "Queue is empty!" << std::endl;

  return 0;
}

int read_sham() {
  sham::SharedMemoryBuffer shared_memory_buffer("/my_memory", kBufferSize,
                                                sham::SharedMemoryBuffer::Type::kRead);
  UintQueue* q = shared_memory_buffer.As<UintQueue>();

  TRACE_VAR(q->size());
  TRACE_VAR(q->capacity());
  TRACE_VAR(q->empty());
  while (!q->empty()) {
    uint64_t i;
    q->pop(i);
    TRACE_VAR(i);
  }

  return 0;
}

int read_sham_in_loop() {
  while (true) {
    sham::SharedMemoryBuffer shared_memory_buffer("/my_memory", kBufferSize,
                                                sham::SharedMemoryBuffer::Type::kRead);
    UintQueue* q = shared_memory_buffer.As<UintQueue>();
    TRACE_VAR(q->size());
    TRACE_VAR(q->capacity());
    TRACE_VAR(q->empty());
    size_t num_pops = 100;
    while (--num_pops > 0) {
      uint64_t i;
      q->pop(i);
      TRACE_VAR(i);
    }
  }

  return 0;
}

int main(int argc, char** argv) {
  std::cout << "hello, sham!\n";
  if (argc == 1)
    create_sham();
  else
    read_sham_in_loop();
}
