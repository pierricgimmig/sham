/*
MIT License - Copyright (c) 2025 Pierric Gimmig
 */

#pragma once

#include <atomic>
#include <stdexcept>
#include <span>
#include <vector>

#define CHECK(x) x

namespace sham {

// Bounded shared-memory-friendly lock-free variable-sized elements MPMC queue.
template <size_t kCapacity>
class MpmcQueue {
 public:
  struct BlockHeader {
    std::atomic<int> size = 0;
  };

  explicit MpmcQueue() : head_(0), tail_(0) {
    std::memset(data_, 0, sizeof(data_));
  }
  ~MpmcQueue() noexcept {}
  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  bool try_push(std::span<uint8_t> data) noexcept {
    size_t block_size = align_to_cache_line(data.size() + sizeof(BlockHeader));
    while (true) {
      // Conservatively estimate free space, return false if insufficient
      size_t tail = tail_.load(std::memory_order_relaxed);
      size_t head = head_.load(std::memory_order_acquire);
      if ((head + block_size + sizeof(BlockHeader) - tail) > kCapacity) {
        if (try_shrink()) continue;
        return false;
      }
      // Try to acquire write block by advancing head
      if (head_.compare_exchange_weak(head, head + block_size, std::memory_order_release,
                                      std::memory_order_acquire)) {
        // Block acquired for write, initialize next block header
        auto next_header = reinterpret_cast<BlockHeader*>(&data_[idx(head + block_size)]);
        next_header->size.store(0, std::memory_order_relaxed);
        // Store data, always contiguous thanks to mapping memory space twice
        auto header = reinterpret_cast<BlockHeader*>(&data_[idx(head)]);
        std::memcpy(static_cast<void*>(header + 1), data.data(), data.size());
        // Publish block to consumer by setting size, which must be zero initially
        CHECK(header->size.compare_exchange_strong(0, data.size(), std::memory_order_release));
        return true;
      }
    }
  }

  bool try_pop(std::vector<uint8_t>& buffer) noexcept {
    size_t read = read_.load(std::memory_order_acquire);
    BlockHeader* header = reinterpret_cast<BlockHeader*>(&data_[idx(read)]);
    size_t size = header->size.load(std::memory_order_acquire);
    if (size == 0) return false;
    size_t block_size = align_to_cache_line(size + sizeof(BlockHeader));
    if (read_.compare_exchange_strong(read, read + block_size, std::memory_order_release,
                                      std::memory_order_relaxed)) {
      // Block acquired for read
      buffer.resize(size);
      std::memcpy(buffer.data(), static_cast<void*>(header + 1), size);
      // Mark block as free
      header->size.store(-static_cast<int>(block_size), std::memory_order_release);
      try_shrink();
      return true;
    }
    return false;
  }

  bool try_shrink() {
    size_t tail = tail_.load(std::memory_order_acquire);
    while (true) {
      BlockHeader* header = reinterpret_cast<BlockHeader*>(&data_[idx(tail)]);
      int size = header->size.load(std::memory_order_acquire);
      if ((size > 0) || !tail_.compare_exchange_strong(
                            tail, tail + align_to_cache_line(-size + sizeof(BlockHeader)),
                            std::memory_order_release, std::memory_order_acquire)) {
        return false;
      }
    }
    return true;
  }

  size_t size() const noexcept {
    return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
  }
  bool empty() const noexcept { return size() == 0; }

 private:
  static constexpr void validate(){
    static_assert(kCapacity >= kCacheLineSize, "kCapacity must be at least one cache line");
    static_assert((kCapacity & (kCapacity - 1)) == 0, "kCapacity must be a power of 2");
  }

  static constexpr size_t kCacheLineSize = 128;
  constexpr inline size_t idx(size_t i) const noexcept { return i & (kCapacity - 1); }
  constexpr inline size_t align_to_cache_line(size_t size) {
    return (size + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
  }

 private:
  alignas(kCacheLineSize) uint8_t data_[kCapacity];
  alignas(kCacheLineSize) std::atomic<size_t> head_;
  alignas(kCacheLineSize) std::atomic<size_t> tail_;
  alignas(kCacheLineSize) std::atomic<size_t> read_;
};

}  // namespace sham

// Q: which cache line should the header be in? A: probably the same one as the data.
// Q: explicitly explain why header is pre-allocated: we need a preexisting data to wait on while
// the size is being written