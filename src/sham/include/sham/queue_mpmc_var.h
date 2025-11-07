/*
MIT License - Copyright (c) 2025 Pierric Gimmig
 */

#pragma once

#include <atomic>
#include <span>
#include <stdexcept>
#include <vector>

#define CHECK(x) x

namespace sham {

// Bounded shared-memory-friendly lock-free variable-sized elements MPMC queue.
template <size_t kCapacity>
class MpmcQueue {
 public:
  struct Header {
    std::atomic<int> size = 0;
  };

  explicit MpmcQueue() : head_(0), tail_(0) { std::memset(data_, 0, sizeof(data_)); }
  ~MpmcQueue() noexcept {}
  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  bool try_push(std::span<uint8_t> data) noexcept {
    size_t block_size = align_to_cache_line(data.size() + sizeof(Header));
    while (true) {
      // Conservatively estimate free space, return false if insufficient
      size_t tail = tail_.load(std::memory_order_relaxed);
      size_t head = head_.load(std::memory_order_acquire);
      if ((head + block_size + sizeof(Header) - tail) > kCapacity) {
        // Queue is too full for new block, try shrinking
        if (shrink()) continue;
        return false;
      }
      // Try to acquire write block by advancing head
      if (head_.compare_exchange_weak(head, head + block_size, std::memory_order_release,
                                      std::memory_order_acquire)) {
        // Block acquired for write, initialize next block header
        Header* next_header = get_header(head + block_size);
        next_header->size.store(0, std::memory_order_relaxed);
        // Store data, always contiguous thanks to mapping memory space twice
        Header* header = get_header(head);
        std::memcpy(static_cast<void*>(header + 1), data.data(), data.size());
        // Publish block to consumer by setting size, which must be zero initially
        int expected = 0;
        CHECK(header->size.compare_exchange_strong(expected, static_cast<int>(data.size()), std::memory_order_release));
        return true;
      }
    }
  }

  bool try_pop(std::vector<uint8_t>& buffer) noexcept {
    size_t read = read_.load(std::memory_order_acquire);
    Header* header = get_header(read);
    size_t size = header->size.load(std::memory_order_acquire);
    if (size == 0) return false;
    size_t block_size = align_to_cache_line(size + sizeof(Header));
    if (read_.compare_exchange_strong(read, read + block_size, std::memory_order_release,
                                      std::memory_order_relaxed)) {
      // Block acquired for read, consume data
      buffer.resize(size);
      std::memcpy(buffer.data(), static_cast<void*>(header + 1), size);
      // Mark block as free
      header->size.store(-static_cast<int>(block_size), std::memory_order_release);
      shrink();
      return true;
    }
    return false;
  }

  size_t shrink() noexcept {
    size_t space_reclaimed = 0;
    size_t tail = tail_.load(std::memory_order_acquire);
    while (true) {
      Header* header = get_header(tail);
      int size = header->size.load(std::memory_order_acquire);
      if (size >= 0) break;
      size_t new_tail = tail + align_to_cache_line(-size + sizeof(Header));
      if (tail_.compare_exchange_strong(tail, new_tail, std::memory_order_release,
                                         std::memory_order_acquire)) {
        space_reclaimed += (new_tail - tail);
      } else {
        break;
      }
    }
    return space_reclaimed;
  }

  size_t size() const noexcept {
    return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
  }
  bool empty() const noexcept { return size() == 0; }
  std::string description() { return "Variable-sized MPMC queue"; }

 private:
  static constexpr void validate() {
    static_assert(kCapacity >= kCacheLineSize, "kCapacity must be at least one cache line");
    static_assert((kCapacity & (kCapacity - 1)) == 0, "kCapacity must be a power of 2");
  }

  static constexpr size_t kCacheLineSize = 128;
  constexpr inline size_t idx(size_t i) const noexcept { return i & (kCapacity - 1); }
  constexpr inline size_t align_to_cache_line(size_t size) {
    return (size + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
  }
  constexpr inline Header* get_header(size_t index) noexcept {
    return reinterpret_cast<Header*>(&data_[idx(index)]);
  }

  alignas(kCacheLineSize) std::atomic<size_t> head_;
  alignas(kCacheLineSize) std::atomic<size_t> tail_;
  alignas(kCacheLineSize) std::atomic<size_t> read_;
  alignas(kCacheLineSize) uint8_t data_[kCapacity];
  // should map the data_ buffer twice in memory
};

}  // namespace sham
