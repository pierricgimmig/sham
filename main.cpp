#include <iostream>
#include "SPSCQueue.h"
#include "ShamQueue.h"
#include "ShamQueueMPMC.h"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <assert.h>
#include <memory>

#define TRACE_VAR(x) std::cout << #x << " = " << x << std::endl

class SharedMemoryBuffer
{
public:
    enum class Type
    {
        kCreate,
        kRead,
        kInvalid
    };
    SharedMemoryBuffer(std::string_view name, size_t capacity, Type type) : name_(name), capacity_(capacity), type_(type)
    {
        assert(type_ != Type::kInvalid);
        valid_ = type_ == Type::kCreate ? Create() : Read();
        assert(valid_);
    }

    ~SharedMemoryBuffer()
    {
        munmap(buffer_, capacity_);
        shm_unlink(name_.c_str());
    }

    SharedMemoryBuffer() = delete;
    SharedMemoryBuffer(const SharedMemoryBuffer &) = delete;
    SharedMemoryBuffer &operator=(const SharedMemoryBuffer &) = delete;

    bool Create()
    {
        fd_ = shm_open(name_.c_str(), O_RDWR | O_CREAT, 0600);
        if (fd_ == -1)
        {
            perror("Can't open memory fd");
            return false;
        }

        if ((ftruncate(fd_, capacity_)) == -1)
        {
            perror("Can't truncate memory");
            return false;
        }

        /* map memory using our file descriptor */
        void *ptr = mmap(NULL, capacity_, PROT_WRITE, MAP_SHARED, fd_, 0);
        if (ptr == MAP_FAILED)
        {
            perror("Memory mapping failed");
            return false;
        }

        buffer_ = static_cast<uint8_t *>(ptr);
        return true;
    }

    bool Read()
    {
        fd_ = shm_open(name_.c_str(), O_RDWR, 0600);
        if (fd_ == -1)
        {
            perror("Can't open file descriptor");
            return 1;
        }

        void* ptr = mmap(NULL, capacity_, PROT_READ|PROT_WRITE, MAP_SHARED, fd_, 0);
        if (ptr == MAP_FAILED)
        {
            perror("Memory mapping failed");
            return 1;
        }

        buffer_ = static_cast<uint8_t *>(ptr);
        return true;
    }

    uint8_t *Allocate(size_t num_bytes)
    {
        assert(type_ == Type::kCreate);
        size_t next_size = size_ + num_bytes;
        if (next_size > capacity_)
            return nullptr;
        uint8_t *ptr = buffer_ + size_;
        size_ = next_size;
        return ptr;
    }

    uint8_t* data() { return buffer_; }

    size_t capacity() const { return capacity_; }
    size_t size() const { return size_; }
    bool valid() const { return valid_; }

private:
    int fd_ = -1;
    std::string name_;
    uint8_t *buffer_ = nullptr;
    size_t size_ = 0;
    size_t capacity_ = 0;
    bool valid_ = false;
    Type type_ = Type::kInvalid;
};

const size_t kBufferSize = 8 * 1024 * 1024;
//using UintQueue = rigtorp::SPSCQueue<uint32_t, Allocator<uint32_t>>;
//using UintQueue = sham::SPSCQueue<uint32_t, 64*1024>;
using UintQueue = sham::mpmc::Queue<uint32_t, 64*1024>;

int create_sharm()
{   
    SharedMemoryBuffer shared_memory_buffer("/my_memory", kBufferSize, SharedMemoryBuffer::Type::kCreate);

    TRACE_VAR(shared_memory_buffer.size());
    void *buffer = shared_memory_buffer.Allocate(sizeof(UintQueue));
    TRACE_VAR(buffer);
    TRACE_VAR(shared_memory_buffer.size());

    UintQueue *q = new (buffer)(UintQueue)();

    TRACE_VAR(sizeof(UintQueue));
    TRACE_VAR(shared_memory_buffer.size());
    TRACE_VAR(shared_memory_buffer.size() - sizeof(UintQueue));
    TRACE_VAR(shared_memory_buffer.capacity());

    uint32_t i = 0;
    while (q->size() != q->capacity()/2)
        q->push(++i);

    std::cout << "Queue has: " << q->size() << " elements" << std::endl;
    
    while(!q->empty()){}

    std::cout << "Queue is empty!" << std::endl;

    return 0;
}

int read_sharm()
{
    SharedMemoryBuffer shared_memory_buffer("/my_memory", kBufferSize, SharedMemoryBuffer::Type::kRead);

    //UintQueue* q = reinterpret_cast<UintQueue*>(shared_memory_buffer.data());
    UintQueue* q = reinterpret_cast<UintQueue*>(shared_memory_buffer.data());
    /* open memory file descriptor */
    
    TRACE_VAR(q->size());
    TRACE_VAR(q->capacity());
    TRACE_VAR(q->empty());
    while(!q->empty()){
        //uint32_t i = *q->front();
        uint32_t i;
        q->pop(i);
        TRACE_VAR(i);
        //q->pop();
    }
   
    return 0;
}

int main(int argc, char **argv)
{
    std::cout << "hello, world!\n";
    if (argc == 1)
        create_sharm();
    else
        read_sharm();
}
