/*
MIT License - Copyright (c) 2025 Pierric Gimmig
 */

#pragma once

#include <atomic>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

namespace sham {

// Bounded shared-memory-friendly variable-sized elements MPMC queue.
template <size_t kCapacity>
class MpmcQueue {
 public:
  struct Header {
    std::atomic<int32_t> size = 0;
  };

  explicit MpmcQueue() : head_(1), tail_(0), read_(0) {
    static_assert(kCapacity >= kCacheLineSize, "kCapacity must be at least one cache line");
    static_assert((kCapacity & (kCapacity - 1)) == 0, "kCapacity must be a power of 2");
    std::memset(data_, 0, sizeof(data_));
  }
  ~MpmcQueue() noexcept {}
  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  bool try_push(std::span<uint8_t> data) noexcept {
    size_t block_size = align_to_cache_line(data.size() + sizeof(Header));
    for (;;) {
      // Compute free space lower bound, return false if insufficient
      size_t tail = tail_.load(std::memory_order_acquire);
      size_t head = head_.load(std::memory_order_acquire) & ~size_t(1);
      if ((head + block_size + sizeof(Header) - tail) > kCapacity) {
        return false;
      }
      // Try to acquire write block by advancing head
      size_t new_head = head + block_size;
      // We can only advance head once it's been incremented (next size has been set to zero)
      head += 1;
      if (head_.compare_exchange_strong(head, new_head, std::memory_order_acq_rel)) {
        // Initialize next header while we have exclusivity
        Header* next_header = get_header(new_head);
        next_header->size.store(0, std::memory_order_relaxed);
        head_.store(new_head + 1, std::memory_order_release);

        // Write, handling wrap-around, then publish to consumer by setting size
        Header* header = get_header(head);
        write(reinterpret_cast<uint8_t*>(header + 1), data);
        header->size.store(static_cast<int>(data.size()), std::memory_order_release);
        return true;
      }
    }
  }

  bool try_pop(std::vector<uint8_t>& buffer) noexcept {
    size_t read = read_.load(std::memory_order_acquire);
    Header* header = get_header(read);
    // the current header size is only valid once head_ has been incremented
    while (head_.load(std::memory_order_acquire) == read)
      ;
    int size = header->size.load(std::memory_order_acquire);
    if (size <= 0) return false;
    size_t new_read = read + align_to_cache_line(size + sizeof(Header));
    if (read_.compare_exchange_strong(read, new_read, std::memory_order_acq_rel)) {
      // Item acquired for read, consume handling wrap-around
      this->read(reinterpret_cast<uint8_t*>(header + 1), size, buffer);
      // Mark block as free
      header->size.store(-size, std::memory_order_release);
      shrink();
      return true;
    }
    return false;
  }

  inline size_t shrink() noexcept {
    size_t space_reclaimed = 0;
    for (;;) {
      size_t tail = tail_.load(std::memory_order_acquire);
      Header* header = get_header(tail);
      int size = header->size.load(std::memory_order_acquire);
      if (size >= 0) break;
      size_t new_tail = tail + align_to_cache_line(-size + sizeof(Header));
      if (!tail_.compare_exchange_strong(tail, new_tail, std::memory_order_acq_rel)) break;
      space_reclaimed += (new_tail - tail);
    }
    return space_reclaimed;
  }

  size_t size() noexcept {
    shrink(); 
    return (head_.load(std::memory_order_acquire) & ~size_t(1)) -
           tail_.load(std::memory_order_acquire);
  }
  bool empty() noexcept { return size() == 0; }

  static constexpr inline size_t align_to_cache_line(size_t size) {
    return (size + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
  }

 private:
  static constexpr size_t kCacheLineSize = 128;
  static constexpr inline size_t idx(size_t i) noexcept { return i & (kCapacity - 1); }
  constexpr inline Header* get_header(size_t index) noexcept {
    index = (index & ~size_t(1));
    return reinterpret_cast<Header*>(&data_[idx(index)]);
  }

  inline void write(uint8_t* dest, std::span<const uint8_t> data) noexcept {
    size_t size = std::min<size_t>((data_ + kCapacity) - dest, data.size());
    std::memcpy(dest, data.data(), size);
    if (size < data.size()) {
      std::memcpy(&data_[0], data.data() + size, data.size() - size);
    }
  }

  inline void read(uint8_t* src, size_t size, std::vector<uint8_t>& buffer) {
    buffer.resize(size);
    size_t chunk_size = std::min<size_t>((data_ + kCapacity) - src, size);
    std::memcpy(buffer.data(), src, chunk_size);
    if (chunk_size < size) {
      std::memcpy(buffer.data() + chunk_size, data_, size - chunk_size);
    }
  }

  alignas(kCacheLineSize) std::atomic<size_t> head_;
  alignas(kCacheLineSize) std::atomic<size_t> tail_;
  alignas(kCacheLineSize) std::atomic<size_t> read_;
  alignas(kCacheLineSize) uint8_t data_[kCapacity];
};

}  // namespace sham
