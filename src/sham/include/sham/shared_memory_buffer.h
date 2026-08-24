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

#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>
#include <utility>

#include "sham/shared_memory.h"

namespace sham {

// Creates named shared memory buffer of specified capacity. The shared memory object is unlinked
// when the creating SharedMemoryBuffer is destroyed. Processes that only access an existing
// mapping close their handle without unlinking the name.
class SharedMemoryBuffer {
 public:
  enum class Type { kInvalid, kCreate, kAccessExisting };

  SharedMemoryBuffer(std::string_view name, size_t capacity, Type type)
      : handle_(kInvalidFileHandle),
        name_(name),
        buffer_(nullptr),
        size_(0),
        capacity_(capacity),
        type_(type) {
    handle_ = type == Type::kCreate ? sham::CreateFileMapping(name, capacity)
                                    : sham::OpenFileMapping(name);
    if (handle_ == kInvalidFileHandle) {
      buffer_ = nullptr;
      return;
    }
    buffer_ = sham::MapViewOfFile(handle_, capacity_);
  }

  SharedMemoryBuffer(SharedMemoryBuffer&& other) noexcept
      : handle_(other.handle_),
        name_(std::move(other.name_)),
        buffer_(other.buffer_),
        size_(other.size_),
        capacity_(other.capacity_),
        type_(other.type_) {
    other.handle_ = kInvalidFileHandle;
    other.buffer_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
    other.type_ = Type::kInvalid;
  }

  SharedMemoryBuffer& operator=(SharedMemoryBuffer&& other) noexcept {
    if (this != &other) {
      this->~SharedMemoryBuffer();
      new (this) SharedMemoryBuffer(std::move(other));
    }
    return *this;
  }

  ~SharedMemoryBuffer() {
    sham::UnMapViewOfFile(buffer_, capacity_);
    if (type_ == Type::kCreate) {
      sham::DestroyFileMapping(handle_, name_);
    } else {
      sham::CloseFileMapping(handle_);
    }
    handle_ = kInvalidFileHandle;
    buffer_ = nullptr;
    type_ = Type::kInvalid;
  }

  SharedMemoryBuffer() = delete;
  SharedMemoryBuffer(const SharedMemoryBuffer&) = delete;
  SharedMemoryBuffer& operator=(const SharedMemoryBuffer&) = delete;

  uint8_t* Allocate(size_t num_bytes, size_t alignment = alignof(std::max_align_t)) {
    if (buffer_ == nullptr || alignment == 0 || (alignment & (alignment - 1)) != 0) {
      return nullptr;
    }
    const size_t aligned_size = (size_ + (alignment - 1)) & ~(alignment - 1);
    const size_t next_size = aligned_size + num_bytes;
    if (next_size > capacity_) return nullptr;
    uint8_t* ptr = buffer_ + aligned_size;
    size_ = next_size;
    return ptr;
  }

  template <typename T, typename... Args>
  T* Allocate(Args&&... args) {
    void* buffer = Allocate(sizeof(T), alignof(T));
    if (buffer == nullptr) return nullptr;
    return new (buffer) T(std::forward<Args>(args)...);
  }

  template <typename T>
  T* As(size_t offset = 0) {
    if (buffer_ == nullptr || offset + sizeof(T) > capacity_) return nullptr;
    return reinterpret_cast<T*>(buffer_ + offset);
  }

  uint8_t* data() { return buffer_; }
  size_t capacity() const { return capacity_; }
  size_t size() const { return size_; }
  bool valid() const { return handle_ != kInvalidFileHandle && buffer_ != nullptr; }
  Type type() const { return type_; }

 private:
  FileHandle handle_ = kInvalidFileHandle;
  std::string name_;
  uint8_t* buffer_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
  Type type_ = Type::kInvalid;
};

}  // namespace sham
