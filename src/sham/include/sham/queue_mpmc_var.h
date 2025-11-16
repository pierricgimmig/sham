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

  explicit MpmcQueue() : head_(1), tail_(0), read_(0) {
    static_assert(kCapacity >= kCacheLineSize, "kCapacity must be at least one cache line");
    static_assert((kCapacity & (kCapacity - 1)) == 0, "kCapacity must be a power of 2");
    std::memset(data_, 0, sizeof(data_));
    *reinterpret_cast<int*>(data_) = kReadyToFill;
  }
  ~MpmcQueue() noexcept {}
  MpmcQueue(const MpmcQueue&) = delete;
  MpmcQueue& operator=(const MpmcQueue&) = delete;

  bool try_push(std::span<uint8_t> data) noexcept {
    constexpr size_t mask = ~static_cast<size_t>(3);
    size_t block_size = align_to_cache_line(data.size() + sizeof(Header));
    for(;;) {
      // Conservatively estimate free space, return false if insufficient
      size_t tail = tail_.load(std::memory_order_acquire);
      size_t head = head_.load(std::memory_order_acquire) & mask;
      if ((head + block_size + sizeof(Header) - tail) > kCapacity) {
        // Queue is too full for new block, try shrinking
        if (shrink()) continue;
        return false;
      }
      // Try to acquire write block by advancing head
      size_t new_head = head + block_size;
      // We can only advance head once it's been incremented (size has been set)
      head = head + 1;
      if (head_.compare_exchange_strong(head, new_head, std::memory_order_acq_rel)) {
        ++num_pushs_;
        // Initialize header while we have head exclusivity
        Header* next_header = get_header(new_head);
        next_header->size.store(0, std::memory_order_release);
        head_.store(new_head + 1, std::memory_order_release);

        Header* header = get_header(head);
        std::memcpy(static_cast<void*>(header + 1), data.data(), data.size());
        // Publish block to consumer by setting size
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
    static volatile int do_print = 0;
    if (do_print > 0) {
      print();
      --do_print;
    }
    if (size <= 0) return false;
    size_t new_read = read + align_to_cache_line(size + sizeof(Header));
    if (read_.compare_exchange_strong(read, new_read, std::memory_order_acq_rel)) {
      ++num_pops_;
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
    for (;;) {
      size_t tail = tail_.load(std::memory_order_acquire);
      Header* header = get_header(tail);
      int size = header->size.load(std::memory_order_acquire);
      if (size >= 0) break;
      size_t new_tail = tail + align_to_cache_line(-size + sizeof(Header));
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
    size_t num_displayed_elems = 32;
    size_t head = head_.load();
    size_t read = read_.load();
    size_t tail = tail_.load();

    size_t start = tail >= num_displayed_elems*elem_size / 2 ? tail - num_displayed_elems*elem_size / 2 : 0;
    std::cout << "num pops: " << num_pops_ << std::endl;
    for (size_t i = start; i < start + num_displayed_elems; i += elem_size) {
      
      std::cout << std::setw(5) << i << "|";
    }
    std::cout << std::endl;
    for (size_t i = start; i < start + num_displayed_elems*elem_size; i+=elem_size) {
      Header* header = get_header(i);
      std::cout << std::setw(5) << header->size.load() << "|";
    }
    std::cout << std::endl;
    std::cout << "head: " << idx(head) / 128.f << " (" << head / 128.f << ")" << std::endl;
    std::cout << "read: " << idx(read) / 128.f << " (" << read / 128.f << ")" << std::endl;
    std::cout << "tail: " << idx(tail) / 128.f << " (" << tail / 128.f << ")" << std::endl;
  }

  size_t size() noexcept {
    shrink();
    constexpr size_t mask = ~static_cast<size_t>(3);
    return head_.load(std::memory_order_acquire)&mask - tail_.load(std::memory_order_acquire);
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
    index = (index & ~static_cast<size_t>(3));
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

  static constexpr int kReadyToFill = 0;
  size_t capacity_ = kCapacity;  // debug
  std::atomic<size_t> num_pushs_ = 0;
  std::atomic<size_t> num_pops_ = 0;
  alignas(kCacheLineSize) std::atomic<size_t> head_;
  alignas(kCacheLineSize) std::atomic<size_t> tail_;
  alignas(kCacheLineSize) std::atomic<size_t> read_;
  alignas(kCacheLineSize) uint8_t data_[kCapacity];
  // should map the data_ buffer twice in memory
};

}  // namespace sham
