#pragma once

namespace sham {

class SharedMemoryBuffer {
 public:
  enum class Type { kCreate, kRead, kInvalid };
  SharedMemoryBuffer(std::string_view name, size_t capacity, Type type)
      : name_(name), capacity_(capacity), type_(type) {
    assert(type_ != Type::kInvalid);
    valid_ = type_ == Type::kCreate ? Create() : Read();
    assert(valid_);
  }

  ~SharedMemoryBuffer() {
    munmap(buffer_, capacity_);
    if(type_ == Type::kCreate)
        shm_unlink(name_.c_str());
  }

  SharedMemoryBuffer() = delete;
  SharedMemoryBuffer(const SharedMemoryBuffer&) = delete;
  SharedMemoryBuffer& operator=(const SharedMemoryBuffer&) = delete;

  bool Create() {
    fd_ = shm_open(name_.c_str(), O_RDWR | O_CREAT, 0600);
    if (fd_ == -1) {
      perror("Can't open memory fd");
      return false;
    }

    if ((ftruncate(fd_, capacity_)) == -1) {
      perror("Can't truncate memory");
      return false;
    }

    /* map memory using our file descriptor */
    void* ptr = mmap(NULL, capacity_, PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
      perror("Memory mapping failed");
      return false;
    }

    buffer_ = static_cast<uint8_t*>(ptr);
    return true;
  }

  bool Read() {
    fd_ = shm_open(name_.c_str(), O_RDWR, 0600);
    if (fd_ == -1) {
        std::cout <<  strerror(errno) << std::endl;
      perror("Can't open file descriptor");
      return 1;
    }

    void* ptr = mmap(NULL, capacity_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
    if (ptr == MAP_FAILED) {
      perror("Memory mapping failed");
      return 1;
    }

    buffer_ = static_cast<uint8_t*>(ptr);
    return true;
  }

  uint8_t* Allocate(size_t num_bytes) {
    assert(type_ == Type::kCreate);
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
  bool valid() const { return valid_; }

 private:
  int fd_ = -1;
  std::string name_;
  uint8_t* buffer_ = nullptr;
  size_t size_ = 0;
  size_t capacity_ = 0;
  bool valid_ = false;
  Type type_ = Type::kInvalid;
};

}  // namespace sham