/*
MIT License - Copyright (c) 2025 Pierric Gimmig
 */

#pragma once

#include <atomic>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>

void do_abort() {
  // abort
  abort();
}

// #define CHECK
#define CHECK(condition)                                                                   \
  do {                                                                                     \
    if (!(condition)) {                                                                    \
      std::cerr << "CHECK failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ \
                << std::endl;                                                              \
      do_abort();                                                                          \
    }                                                                                      \
  } while (false)

namespace sham {

// Bounded shared-memory-friendly lock-free variable-sized elements MPMC queue.
template <size_t kCapacity>
class MpmcQueue {
 public:
  struct Header {
    std::atomic<int32_t> size = 0;
  };

  explicit MpmcQueue() : head_(0), tail_(0), read_(0) {
    static_assert(kCapacity >= kCacheLineSize, "kCapacity must be at least one cache line");
    static_assert((kCapacity & (kCapacity - 1)) == 0, "kCapacity must be a power of 2");
    std::memset(data_, 0, sizeof(data_));
  }
  ~MpmcQueue() noexcept {}
  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  bool try_push(std::span<uint8_t> data) noexcept {
    size_t block_size = align_to_cache_line(data.size() + sizeof(Header));
    while (true) {
      // Conservatively estimate free space, return false if insufficient
      size_t tail = tail_.load(std::memory_order_acquire);
      size_t head = head_.load(std::memory_order_acquire);
      if ((head + block_size + sizeof(Header) - tail) > kCapacity) {
        // Queue is too full for new block, try shrinking
        if (shrink()) continue;
        return false;
      }
      // Try to acquire write block by advancing head
      if (head_.compare_exchange_strong(head, head + block_size, std::memory_order_acq_rel)) {
        // Block acquired for write, initialize next block header
        Header* next_header = get_header(head + block_size);
        next_header->size.store(0, std::memory_order_release);
        // Store data, always contiguous thanks to mapping memory space twice
        Header* header = get_header(head);
        std::memcpy(static_cast<void*>(header + 1), data.data(), data.size());
        // Publish block to consumer by setting size, which must be zero initially
        while (header->size.load(std::memory_order_acquire) != 0);
        int expected = 0;
        if (!header->size.compare_exchange_strong(expected, static_cast<int>(data.size()),
                                                  std::memory_order_acq_rel)) {
          std::cout << "CAS failed, got " << expected << " instead of 0 when reading idx "
                    << idx(head) / 128 << std::endl;
          print();
          exit(1);
        }
        return true;
      }
    }
  }

  bool try_pop(std::vector<uint8_t>& buffer) noexcept {
    size_t read = read_.load(std::memory_order_acquire);
    Header* header = get_header(read);
    int size = header->size.load(std::memory_order_acquire);
    if (size <= 0) return false;
    size_t block_size = align_to_cache_line(size + sizeof(Header));
    if (read_.compare_exchange_strong(read, read + block_size, std::memory_order_acq_rel)) {
      ++num_pops;
      // if ((num_pops % 1999) == 0) print();
      // Block acquired for read, consume data
      buffer.resize(size);
      std::memcpy(buffer.data(), static_cast<void*>(header + 1), size);
      // Mark block as free
      header->size.store(-size, std::memory_order_release);
      if (!shrink()) {
        // print();
      }
      return true;
    }
    return false;
  }

  inline size_t shrink() noexcept {
    // CHECK(size() <= kCapacity); // this has fired once
    size_t space_reclaimed = 0;
    size_t tail = tail_.load(std::memory_order_acquire);
    while (true) {
      Header* header = get_header(tail);
      int size = header->size.load(std::memory_order_acquire);
      if (size >= 0) break;
      size_t new_tail = tail + align_to_cache_line(-size + sizeof(Header));
      CHECK(new_tail <= read_.load(std::memory_order_acquire));
      if (tail_.compare_exchange_strong(tail, new_tail, std::memory_order_acq_rel)) {
        space_reclaimed += (new_tail - tail);
      } else {
        break;
      }
    }
    return space_reclaimed;
  }

  void print() {
    size_t elem_size = 128;
    size_t num_elems = kCapacity / elem_size;
    std::cout << "num elements: " << num_elems << std::endl;
    std::cout << "num pops: " << num_pops << std::endl;
    for (size_t i = 0; i < num_elems; ++i) {
      std::cout << std::setw(2) << i << "|";
    }
    std::cout << std::endl;
    for (size_t i = 0; i < num_elems; ++i) {
      uint8_t* ptr = &data_[i * elem_size];
      Header* header = reinterpret_cast<Header*>(ptr);
      std::cout << std::setw(2) << header->size.load() << "|";
    }
    std::cout << std::endl;
    size_t head = head_.load();
    size_t read = read_.load();
    size_t tail = tail_.load();
    std::cout << "head: " << idx(head) / 128.f << " (" << head / 128.f << ")" << std::endl;
    std::cout << "read: " << idx(read) / 128.f << " (" << read / 128.f << ")" << std::endl;
    std::cout << "tail: " << idx(tail) / 128.f << " (" << tail / 128.f << ")" << std::endl;
  }

  size_t size() const noexcept {
    return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
  }
  bool empty() const noexcept { return size() == 0; }
  std::string description() { return "Variable-sized MPMC queue"; }

  static constexpr inline size_t align_to_cache_line(size_t size) {
    return (size + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
  }

 private:
  static constexpr size_t kCacheLineSize = 128;
  static constexpr inline size_t idx(size_t i) noexcept { return i & (kCapacity - 1); }
  constexpr inline Header* get_header(size_t index) noexcept {
    CHECK((idx(index) & (kCacheLineSize - 1)) == 0);
    return reinterpret_cast<Header*>(&data_[idx(index)]);
  }

  // Helper: write payload at header_index (unmasked index). Handles wrap-around.
  //+  inline void write_payload(size_t header_index, const void* src, size_t len) noexcept {
  //+    size_t off = idx(header_index) + sizeof(Header);
  //+    const uint8_t* s = static_cast<const uint8_t*>(src);
  //+    if (off + len <= kCapacity) {
  //+      std::memcpy(&data_[off], s, len);
  //+    } else {
  //+      size_t first = kCapacity - off;
  //+      std::memcpy(&data_[off], s, first);
  //+      std::memcpy(&data_[0], s + first, len - first);
  //+    }
  //+  }

  size_t capacity_ = kCapacity;  // debug
  size_t num_pops = 0;
  alignas(kCacheLineSize) std::atomic<size_t> head_;
  alignas(kCacheLineSize) std::atomic<size_t> tail_;
  alignas(kCacheLineSize) std::atomic<size_t> read_;
  alignas(kCacheLineSize) uint8_t data_[kCapacity];
  // should map the data_ buffer twice in memory
};

}  // namespace sham
