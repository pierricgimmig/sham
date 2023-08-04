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

#include <stdint.h>

#include <iostream>
#include <mutex>
#include <vector>

namespace sham {
namespace mpmc {

// Locking mpmc queue. The push and pop operations block by busy waiting.
template <typename T, size_t kCapacity>
class LockingQueue {
 public:
  explicit LockingQueue() {
    static_assert(kCapacity > 0);
    static_assert(IsPowerOfTwoMinusOne(kCapacity));
  }
  ~LockingQueue() {}

  // non-copyable and non-movable
  LockingQueue(const LockingQueue&) = delete;
  LockingQueue& operator=(const LockingQueue&) = delete;

  static constexpr bool IsPowerOfTwoMinusOne(std::size_t n) { return (n & (n + 1)) == 0; }

  template <typename... Args>
  bool try_emplace(Args&&... args) {
    std::lock_guard lk(mutex_);
    if (is_full(lk)) return false;
    new (&data_[in_]) T(std::forward<Args>(args)...);
    in_ = inc(in_);
    return true;
  }

  template <typename... Args>
  void emplace(Args&&... args) {
    while (!try_emplace(std::forward<Args>(args)...)) {
    }
  }

  bool try_push(const T& v) { return try_emplace(v); }
  bool try_push(T&& v) noexcept { return try_emplace(std::forward<T>(v)); }

  void push(const T& v) { emplace(v); }

  bool try_pop(T& v) {
    std::lock_guard lk(mutex_);
    if (in_ == out_) return false;
    v = data_[out_];
    out_ = inc(out_);
    return true;
  }

  void pop(T& v) {
    while (!try_pop(v)) {
    }
  }

  [[nodiscard]] inline size_t size() const {
    std::lock_guard lk(mutex_);
    if (is_full(lk)) return kCapacity;
    size_t size = (in_ + kInternalCapacity - out_) % kInternalCapacity;
    return size;
  }

  [[nodiscard]] inline bool empty() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return empty(lk);
  }
  [[nodiscard]] inline bool is_full() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return is_full(lk);
  }
  [[nodiscard]] static inline size_t capacity() { return kCapacity; }

  std::string description() const { return "Locking queue"; }

 private:
  // We need one extra slot to distinguish between full and empty.
  static constexpr size_t kInternalCapacity = kCapacity + 1;

  [[nodiscard]] inline size_t inc(size_t idx) const { return (idx + 1) % kInternalCapacity; }
  [[nodiscard]] inline bool empty(std::lock_guard<std::mutex>&) const { return in_ == out_; }
  [[nodiscard]] inline bool is_full(std::lock_guard<std::mutex>&) const { return inc(in_) == out_; }

 private:
  T data_[kInternalCapacity];
  mutable std::mutex mutex_;
  size_t in_ = 0;
  size_t out_ = 0;
};
}  // namespace mpmc

}  // namespace sham
