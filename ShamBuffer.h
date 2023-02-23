#pragma once

namespace sham {

// Creates named shared memory buffer of specified capacity. The shared memory object is unlinked
// when the SharedMemoryBuffer is destroyed.
class SharedMemoryBuffer {
 public:
  SharedMemoryBuffer(std::string_view name, size_t capacity) : name_(name), capacity_(capacity) {
    fd_ = shm_open(name_.c_str(), O_RDWR | O_CREAT, 0600);
    if (fd_ == -1) {
      perror("Can't open memory fd");
      return;
    }

    if ((ftruncate(fd_, capacity_)) == -1) {
      perror("Can't truncate memory");
      return;
    }

    /* map memory using our file descriptor */
    void* ptr = mmap(NULL, capacity_, PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
      perror("Memory mapping failed");
      return;
    }

    buffer_ = static_cast<uint8_t*>(ptr);
  }

  ~SharedMemoryBuffer() {
    if (buffer_ != nullptr) munmap(buffer_, capacity_);
    if (fd_ != -1) shm_unlink(name_.c_str());
  }

  SharedMemoryBuffer() = delete;
  SharedMemoryBuffer(const SharedMemoryBuffer&) = delete;
  SharedMemoryBuffer& operator=(const SharedMemoryBuffer&) = delete;

  uint8_t* Allocate(size_t num_bytes) {
    size_t next_size = size_ + num_bytes;
    if (next_size > capacity_) return nullptr;
    uint8_t* ptr = buffer_ + size_;
    size_ = next_size;
    return ptr;
  }

  template <typename T, typename... Args>
  T* Allocate(Args&&... args) {
    void* buffer = Allocate(sizeof(T));
    return new (buffer)(T)(std::forward<Args>(args)...);
  }

  template <typename T, typename... Args>
  T* As(size_t offset = 0) {
    if (offset + sizeof(T) > capacity_) return nullptr;
    return reinterpret_cast<T*>(buffer_ + offset);
  }

  uint8_t* data() { return buffer_; }
  size_t capacity() const { return capacity_; }
  size_t size() const { return size_; }
  bool valid() const { return buffer_ != nullptr; }

 private:
  int fd_ = -1;
  std::string name_;
  uint8_t* buffer_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
};

// Mutable view of existing shared memory buffer.
class SharedMemoryBufferView {
 public:
  SharedMemoryBufferView(std::string_view name, size_t capacity)
      : name_(name), capacity_(capacity) {
    fd_ = shm_open(name_.c_str(), O_RDWR, 0600);
    if (fd_ == -1) {
      perror("Can't open file descriptor");
      return;
    }

    void* ptr = mmap(NULL, capacity_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
      perror("Memory mapping failed");
      return;
    }

    buffer_ = static_cast<uint8_t*>(ptr);
  }

  SharedMemoryBufferView() = delete;
  SharedMemoryBufferView(const SharedMemoryBuffer&) = delete;
  SharedMemoryBufferView& operator=(const SharedMemoryBuffer&) = delete;

  ~SharedMemoryBufferView() { munmap(buffer_, capacity_); }

  template <typename T, typename... Args>
  T* As(size_t offset = 0) {
    if (offset + sizeof(T) > capacity_) return nullptr;
    return reinterpret_cast<T*>(buffer_ + offset);
  }

  uint8_t* data() { return buffer_; }
  size_t capacity() const { return capacity_; }
  bool valid() const { return buffer_ != nullptr; }

 private:
  int fd_ = -1;
  std::string name_;
  uint8_t* buffer_ = nullptr;
  size_t capacity_ = 0;
};

}  // namespace sham
