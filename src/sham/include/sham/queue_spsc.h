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

#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <new>  // std::hardware_destructive_interference_size
#include <type_traits>
#include <utility>

namespace sham {

#if defined(__cpp_lib_hardware_interference_size) && !defined(__APPLE__)
inline constexpr size_t kSpscCacheLineSize = std::hardware_destructive_interference_size;
#else
inline constexpr size_t kSpscCacheLineSize = 64;
#endif

// NOTE: This is a copy of https://github.com/rigtorp/SPSCQueue, with the following modifications
// to make it suitable for shared memory use:
//  - Removed allocations for internal slots in favor of in-place storage to avoid pointers in
//  different address spaces.
//  - Removed the capacity_ member variable in favor of kCapacity template argument.
//  - Store slots in uninitialized bytes so T does not need a default constructor and so
//    destruction does not double-destroy live elements.
template <typename T, size_t kCapacity>
class alignas(kSpscCacheLineSize) SPSCQueue {
 public:
  explicit SPSCQueue() {
    static_assert(kCapacity >= 1);
    static_assert(alignof(SPSCQueue<T, kCapacity>) == kSpscCacheLineSize, "");
    static_assert(sizeof(SPSCQueue<T, kCapacity>) >= 3 * kSpscCacheLineSize, "");
    assert(reinterpret_cast<char*>(&readIdx_) - reinterpret_cast<char*>(&writeIdx_) >=
           static_cast<std::ptrdiff_t>(kSpscCacheLineSize));
  }

  ~SPSCQueue() {
    while (front()) {
      pop();
    }
  }

  // non-copyable and non-movable
  SPSCQueue(const SPSCQueue&) = delete;
  SPSCQueue& operator=(const SPSCQueue&) = delete;

  template <typename... Args>
  void emplace(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args&&...>::value) {
    static_assert(std::is_constructible<T, Args&&...>::value,
                  "T must be constructible with Args&&...");
    auto const writeIdx = writeIdx_.load(std::memory_order_relaxed);
    auto nextWriteIdx = writeIdx + 1;
    if (nextWriteIdx == kInternalCapacity) {
      nextWriteIdx = 0;
    }
    while (nextWriteIdx == readIdxCache_) {
      readIdxCache_ = readIdx_.load(std::memory_order_acquire);
    }
    new (slot(writeIdx)) T(std::forward<Args>(args)...);
    writeIdx_.store(nextWriteIdx, std::memory_order_release);
  }

  template <typename... Args>
  [[nodiscard]] bool try_emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible<T, Args&&...>::value) {
    static_assert(std::is_constructible<T, Args&&...>::value,
                  "T must be constructible with Args&&...");
    auto const writeIdx = writeIdx_.load(std::memory_order_relaxed);
    auto nextWriteIdx = writeIdx + 1;
    if (nextWriteIdx == kInternalCapacity) {
      nextWriteIdx = 0;
    }
    if (nextWriteIdx == readIdxCache_) {
      readIdxCache_ = readIdx_.load(std::memory_order_acquire);
      if (nextWriteIdx == readIdxCache_) {
        return false;
      }
    }
    new (slot(writeIdx)) T(std::forward<Args>(args)...);
    writeIdx_.store(nextWriteIdx, std::memory_order_release);
    return true;
  }

  void push(const T& v) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible");
    emplace(v);
  }

  template <typename P,
            typename = typename std::enable_if<std::is_constructible<T, P&&>::value>::type>
  void push(P&& v) noexcept(std::is_nothrow_constructible<T, P&&>::value) {
    emplace(std::forward<P>(v));
  }

  [[nodiscard]] bool try_push(const T& v) noexcept(std::is_nothrow_copy_constructible<T>::value) {
    static_assert(std::is_copy_constructible<T>::value, "T must be copy constructible");
    return try_emplace(v);
  }

  template <typename P,
            typename = typename std::enable_if<std::is_constructible<T, P&&>::value>::type>
  [[nodiscard]] bool try_push(P&& v) noexcept(std::is_nothrow_constructible<T, P&&>::value) {
    return try_emplace(std::forward<P>(v));
  }

  [[nodiscard]] T* front() noexcept {
    auto const readIdx = readIdx_.load(std::memory_order_relaxed);
    if (readIdx == writeIdxCache_) {
      writeIdxCache_ = writeIdx_.load(std::memory_order_acquire);
      if (writeIdxCache_ == readIdx) {
        return nullptr;
      }
    }
    return slot(readIdx);
  }

  void pop() noexcept {
    static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");
    auto const readIdx = readIdx_.load(std::memory_order_relaxed);
    assert(writeIdx_.load(std::memory_order_acquire) != readIdx);
    slot(readIdx)->~T();
    auto nextReadIdx = readIdx + 1;
    if (nextReadIdx == kInternalCapacity) {
      nextReadIdx = 0;
    }
    readIdx_.store(nextReadIdx, std::memory_order_release);
  }

  [[nodiscard]] size_t size() const noexcept {
    std::ptrdiff_t diff =
        writeIdx_.load(std::memory_order_acquire) - readIdx_.load(std::memory_order_acquire);
    if (diff < 0) {
      diff += kInternalCapacity;
    }
    return static_cast<size_t>(diff);
  }

  [[nodiscard]] bool empty() const noexcept {
    return writeIdx_.load(std::memory_order_acquire) == readIdx_.load(std::memory_order_acquire);
  }

  [[nodiscard]] static size_t capacity() noexcept { return kCapacity; }

 private:
  static constexpr size_t kCacheLineSize = kSpscCacheLineSize;
  // Padding to avoid false sharing between slots_ and adjacent allocations
  static constexpr size_t kPadding = (kCacheLineSize - 1) / sizeof(T) + 1;
  // The queue needs one slack element
  static constexpr size_t kInternalCapacity = kCapacity + 1;
  static constexpr size_t kSlotCount = kInternalCapacity + 2 * kPadding;

  T* slot(size_t index) noexcept {
    return std::launder(reinterpret_cast<T*>(slots_ + sizeof(T) * (index + kPadding)));
  }

 private:
  alignas(T) std::byte slots_[sizeof(T) * kSlotCount]{};

  // Align to cache line size in order to avoid false sharing
  // readIdxCache_ and writeIdxCache_ is used to reduce the amount of cache
  // coherency traffic
  alignas(kCacheLineSize) std::atomic<size_t> writeIdx_ = {0};
  alignas(kCacheLineSize) size_t readIdxCache_ = 0;
  alignas(kCacheLineSize) std::atomic<size_t> readIdx_ = {0};
  alignas(kCacheLineSize) size_t writeIdxCache_ = 0;

  // Padding to avoid adjacent allocations to share cache line with
  // writeIdxCache_
  char padding_[kCacheLineSize - sizeof(writeIdxCache_)];
};

}  // namespace sham
