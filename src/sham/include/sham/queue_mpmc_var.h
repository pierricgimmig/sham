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

namespace sham {
namespace mpmc {

static constexpr size_t hardwareInterferenceSize = 64; // assumed
static constexpr size_t kCacheLineMinusOne = hardwareInterferenceSize - 1; // assumed

inline size_t align_to_cache_line(size_t size) {
  return (size + kCacheLineMinusOne) & ~kCacheLineMinusOne;
}
struct BlockMetadata {
  std::atomic<size_t> ready = {0}; // uint8_t
  std::atomic<size_t> next_metadata_ready = {0};
  size_t size = 0;
};

// Lockless variable size elements MPMC queue
template <size_t kCapacity>
class Queue {
 public:
  explicit Queue() : head_(0), tail_(0) {
    std::memset(data_, 0, sizeof(data_));
  }
  ~Queue() noexcept {}
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;

  template <typename... Args>
  void emplace(Args&&... args) noexcept {
    auto const head = head_.fetch_add(1);
    auto& slot = slots_[idx(head)];
    while (turn(head) * 2 != slot.turn.load(std::memory_order_acquire))
      ;
    slot.construct(std::forward<Args>(args)...);
    slot.turn.store(turn(head) * 2 + 1, std::memory_order_release);
  }

  bool try_push(std::span<uint8_t> data) noexcept {
    size_t data_size = align_to_cache_line(sizeof(BlockMetadata) + data.size());
    while (true) {
      size_t tail = tail_.load(std::memory_order_relaxed);  // conservative estimate of free space
      size_t head = head_.load(std::memory_order_acquire);
      if ((head + data_size - tail) > kCapacity) {
        return false;  // not enough space
      }
      size_t prev_head = head;
      if (head_.compare_exchange_strong(head, head + data_size)) {                      // try to acquire space
        BlockMetadata* prev_metadata = static_cast<tBlockMetadata*>(&data_[idx(prev_head)]);
        CHECK(prev_metadata->next_metadata_ready.load(std::memory_order_relaxed) == 0); // we are the only one to write into this slot
        BlockMetadata* metadata = &data_[idx(head)];                                    // from this point on, the space is reserved, others can enqueue after us
        metadata->size = data.size();                                                   // init size
        metadata->ready.store(0, std::memory_order_relaxed);                            // not yet ready for consumer, relaxed since it will be visible to consumer only after next_metada_ready is released
        metadata->next_metadata_ready.store(0, std::memory_order_relaxed);              // init next metadata
        prev_metadata->next_metadata_ready.store(1, std::memory_order_release);         // mark new metadata as ready in previous block, this means that we can now start dequeuing even if the data is not yet fully written, we can access size and next_metadata_ready
        std::memcpy(static_cast<void*>(metadata+1), data.data(), data.size());          // store data
        slot_header->ready.store(1, std::memory_order_release);                         // mark data as ready for consumption
        return true;
      } else {
        size_t prev_head = head;
        head = head_.load(std::memory_order_acquire);
        if (head == prev_head) {
          return false;
        }
      }
    }
  }

    bool try_pop(std::vector<uint8_t> & buffer) noexcept {
      auto tail = tail_.load(std::memory_order_acquire);
      for (;;) {
        auto& slot = slots_[idx(tail)];
        if (turn(tail) * 2 + 1 == slot.turn.load(std::memory_order_acquire)) {
          if (tail_.compare_exchange_strong(tail, tail + 1)) {
            v = slot.move();
            slot.destroy();
            slot.turn.store(turn(tail) * 2 + 2, std::memory_order_release);
            return true;
          }
        } else {
          auto const prevTail = tail;
          tail = tail_.load(std::memory_order_acquire);
          if (tail == prevTail) {
            return false;
          }
        }
      }
    }

    ptrdiff_t size() const noexcept {
      return static_cast<ptrdiff_t>(head_.load(std::memory_order_relaxed) -
                                    tail_.load(std::memory_order_relaxed));
      bool empty() const noexcept { return size() <= 0; }

     private:
      constexpr size_t idx(size_t i) const noexcept { return i % kInternalCapacity; }
      constexpr size_t turn(size_t i) const noexcept { return i / kInternalCapacity; }
      static constexpr size_t kInternalCapacity = kCapacity + 1;

     private:
      uint8_t data_[kInternalCapacity];
      alignas(hardwareInterferenceSize) std::atomic<size_t> head_;
      alignas(hardwareInterferenceSize) std::atomic<size_t> tail_;
    };

  }  // namespace mpmc

}  // namespace sham
