/*
MIT License - Copyright (c) 2025 Pierric Gimmig
 */

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <span>

#define CHECK(cond)

namespace sham {

// Lockless variable size elements MPMC queue
template <size_t kCapacity>
class MpmcQueue {
 public:
  struct BlockHeader {
    std::atomic<size_t> ready = 0;  // block is ready for full consumption
    std::atomic<size_t> size = 0;   // indicates that block header is ready for consumption
  };

  explicit MpmcQueue() : head_(sizeof(BlockHeader)), tail_(sizeof(BlockHeader)) {
    static_assert(kCapacity >= kCacheLineSize, "kCapacity must be at least one cache line");
    static_assert(is_power_of_two(kCapacity), "kCapacity must be a power of 2");
    std::memset(data_, 0, sizeof(data_));
  }
  ~MpmcQueue() noexcept {}
  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  bool try_push(std::span<uint8_t> data) noexcept {
    size_t block_size = align_to_cache_line(data.size() + sizeof(BlockHeader));
    while (true) {
      // ABA protection: conservatively estimate free space, return false if insufficient.
      size_t tail = tail_.load(std::memory_order_relaxed);
      size_t head = head_.load(std::memory_order_acquire);
      if ((head + block_size - tail) > kCapacity) {
        return false;
      }

      // Try to acquire write block by advancing head
      if (head_.compare_exchange_weak(head, head + block_size, std::memory_order_release,
                                      std::memory_order_acquire)) {
        // Block acquired for write, initialize next block header
        auto next_header = reinterpret_cast<BlockHeader*>(&data_[idx(head + block_size)]) - 1;
        next_header->ready.store(0, std::memory_order_relaxed);
        next_header->size.store(0, std::memory_order_relaxed);
        // Allow consumer to acquire block by publishing size
        auto header = reinterpret_cast<BlockHeader*>(&data_[idx(head)]) - 1;
        header->size.store(data.size(), std::memory_order_release);
        // Store data, always contiguous thanks to mapping memory space twice
        CHECK(header->size.load(std::memory_order_relaxed) == 0);
        std::memcpy(static_cast<void*>(header + 1), data.data(), data.size());
        // Allow consumer to read acquired block
        header->ready.store(1, std::memory_order_release);
        return true;
      }
    }
  }

  bool try_pop(std::vector<uint8_t>& buffer) noexcept {
    size_t tail = tail_.load(std::memory_order_acquire);
    while (true) {
      BlockHeader* header = reinterpret_cast<BlockHeader*>(&data_[idx(tail)]) - 1;
      size_t size = header->size.load(std::memory_order_acquire);
      if (size == 0) return false;  // not ready yet
      size_t block_size = align_to_cache_line(size + sizeof(BlockHeader));

      // Try to acquire read block by advancing tail
      if (tail_.compare_exchange_weak(tail, tail + block_size, std::memory_order_release,
                                      std::memory_order_acquire)) {
        // Block acquired for read, ensure enough space in buffer and wait for `ready`
        buffer.resize(size);
        while (header->ready.load(std::memory_order_acquire) == 0);
        // Consume data
        std::memcpy(buffer.data(), static_cast<void*>(header + 1), size);
      }
    }
  }

  size_t size() const noexcept {
    return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
  }
  bool empty() const noexcept { return size() == 0; }

 private:
  static constexpr size_t kCacheLineSize = 128; //std::hardware_destructive_interference_size;
  constexpr inline size_t idx(size_t i) const noexcept { return i & (kCapacity - 1); }
  constexpr inline bool is_power_of_two(size_t size) { return (size & (size - 1)) == 0; }
  constexpr inline size_t align_to_cache_line(size_t size) {
    return (size + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
  }

 private:
  alignas(kCacheLineSize) uint8_t data_[kCapacity];
  alignas(kCacheLineSize) std::atomic<size_t> head_;
  alignas(kCacheLineSize) std::atomic<size_t> tail_;
};

}  // namespace sham

// Q: which cache line should the header be in? A: probably the same one as the data.
// Q: explicitly explain why header is pre-allocated: we need a preexisting data to wait on while
// the size is being written