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

#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "sham/string_format.h"
#include "sham/timer.h"

namespace sham {

struct Element {
  uint64_t thread_id;
  uint64_t timestamp_ns;
  uint64_t value;
};

// Aligned on cacheline boundary to eliminate false sharing when stored contiguously.
struct alignas(64) ThreadResult {
  uint64_t id = 0;
  uint64_t num_operations = 0;
  uint64_t duration_ns = 0;
};

struct Result {
  Result(std::string_view name, size_t size)
      : name(name), size(size), threads(size), results(size) {}
  double MillionOperationsPerSecond() const {
    double seconds = static_cast<double>(duration_ns) * 0.000'000'0001;
    return (static_cast<double>(TotalNumOperations()) / seconds) * 0.000'0001;
  }
  uint64_t TotalNumOperations() const {
    uint64_t num_ops = 0;
    for (const ThreadResult& result : results) num_ops += result.num_operations;
    return num_ops;
  }
  void Print() const {
    for (const ThreadResult& result : results) {
      std::cout << StrFormat("%s[%u/%u]: %u ops\n", name.c_str(), result.id, threads.size(),
                             result.num_operations);
    }
    std::cout << StrFormat("%s total ops: %u\n", name.c_str(), TotalNumOperations());
  }
  std::string name;
  size_t size = 0;
  std::vector<std::thread> threads;
  std::vector<ThreadResult> results;
  uint64_t duration_ns = 0;
};

struct BenchmarkSummary {
  std::string description;
  size_t num_push_threads = 0;
  size_t num_pop_threads = 0;
  double million_push_operations_per_second = 0;
  double million_pop_operations_per_second = 0;
};

struct BenchmarkStats {
  static BenchmarkStats& Get() {
    static BenchmarkStats stats;
    return stats;
  }
  void Print(std::ostream& out = std::cout) const {
    for (const auto& [desc, s] : benchmark_summaries) {
      out << std::setw(32) << desc;
      out << std::setw(8) << StrFormat(" %u %u ", s.num_push_threads, s.num_pop_threads);
      out << StrFormat(" [%.2f/%.2f] Mops/s", s.million_push_operations_per_second,
                       s.million_pop_operations_per_second)
          << std::endl;
    }
  }

  bool Log() const {
    std::ofstream log_file("benchmark_summary.txt", std::ios_base::app | std::ios_base::out);
    if (log_file.fail()) {
      perror("Could not create summary file, aborting.\n");
      return false;
    }

    Print(log_file);
    log_file.close();
    return true;
  }
  std::map<std::string, BenchmarkSummary> benchmark_summaries;

 private:
  BenchmarkStats() = default;
  ~BenchmarkStats() { Log(); }
};

template <typename QueueT>
class Benchmark {
 public:
  Benchmark(size_t num_push_threads, size_t num_pop_threads, size_t num_elements_to_push)
      : num_push_threads_(num_push_threads),
        num_pop_threads_(num_pop_threads),
        num_unregistered_threads_(num_push_threads + num_pop_threads),
        push_result_("push", num_push_threads),
        pop_result_("pop", num_pop_threads),
        num_elements_to_push_(num_elements_to_push) {
    queue_ = std::make_unique<QueueT>();
  }

  void Run() {
    std::thread push_setup_thread(&Benchmark::LaunchPushThreads, this);
    std::thread pop_setup_thread(&Benchmark::LaunchPopThreads, this);
    push_setup_thread.join();
    pop_setup_thread.join();
    Print();

    std::string description = queue_->description();
    BenchmarkSummary& summary = BenchmarkStats::Get().benchmark_summaries[description];
    summary.description = description;
    summary.num_push_threads = num_push_threads_;
    summary.num_pop_threads = num_pop_threads_;
    summary.million_push_operations_per_second = push_result_.MillionOperationsPerSecond();
    summary.million_pop_operations_per_second = pop_result_.MillionOperationsPerSecond();
  }

  void RunSimple() {
    uint64_t id = 4;
    for (uint64_t i = 0; i < num_elements_to_push_; ++i) queue_->push({id, id, i});

    Element element;
    for (uint64_t i = 0; i < num_elements_to_push_; ++i) {
      queue_->try_pop(element);
      std::cout << "i: " << i << " element[" << element.value << "]" << std::endl;
    }
  }

  size_t GetRequestedNumElementsToPush() const { return num_elements_to_push_; }
  size_t GetNumPushedElements() const { return push_result_.TotalNumOperations(); }
  size_t GetNumPoppedElements() const { return pop_result_.TotalNumOperations(); }
  QueueT* GetQueue() const { return queue_.get(); }

 private:
  void LaunchPushThreads() {
    for (size_t i = 0; i < push_result_.results.size(); ++i) {
      push_result_.threads[i] =
          std::thread(&Benchmark::PushThread, this, i, &push_result_.results[i]);
    }
    BusyWaitForAllThreads();
    Timer timer(&push_result_.duration_ns);
    for (auto& thread : push_result_.threads) {
      thread.join();
    }
  }

  void LaunchPopThreads() {
    for (size_t i = 0; i < pop_result_.results.size(); ++i) {
      pop_result_.threads[i] =
          std::thread(&Benchmark::PopThread, this, i + 1, &pop_result_.results[i]);
    }
    Timer timer(&pop_result_.duration_ns);
    for (auto& thread : pop_result_.threads) {
      thread.join();
    }
  }

  void PushThread(size_t id, ThreadResult* result) {
    result->id = id;
    size_t push_per_thread = num_elements_to_push_ / push_result_.threads.size();
    RegisterAndBusyWaitForAllThreads();
    Timer timer(&result->duration_ns);
    for (size_t i = 0; i < push_per_thread; ++i) {
      queue_->push({id, id, i});
      ++result->num_operations;
    }
  }

  void PopThread(size_t id, ThreadResult* result) {
    result->id = id;
    Element element;
    RegisterAndBusyWaitForAllThreads();
    Timer timer(&result->duration_ns);
    while (num_popped_elements_ < num_elements_to_push_) {
      if (queue_->try_pop(element)) {
        ++result->num_operations;
        ++num_popped_elements_;
      }
    }
  }

  void BusyWaitForAllThreads() {
    while (num_unregistered_threads_ > 0) {
    }
  }

  void RegisterAndBusyWaitForAllThreads() {
    --num_unregistered_threads_;
    BusyWaitForAllThreads();
  }

  void Print() {
    std::cout << StrFormat("Type: %s", queue_->description().c_str()) << std::endl;
    std::cout << StrFormat("Threads: %u push, %u pull\n", push_result_.size, pop_result_.size);
    std::cout << StrFormat("Push/Pop rates: %f/%f M/s\n", push_result_.MillionOperationsPerSecond(),
                           pop_result_.MillionOperationsPerSecond());
    push_result_.Print();
    pop_result_.Print();
    // queue_->print();
    std::cout << std::endl;
  }

 private:
  std::unique_ptr<QueueT> queue_;
  std::atomic<size_t> num_elements_to_push_ = {};
  std::atomic<size_t> num_popped_elements_ = {};
  std::atomic<size_t> num_unregistered_threads_ = {};

  size_t num_push_threads_ = 0;
  size_t num_pop_threads_ = 0;

  Result push_result_;
  Result pop_result_;
};

template <typename QueueT>
class BenchmarkVariableSize {
 public:
  static constexpr size_t kMinChunkSize = 1;
  static constexpr size_t kMaxChunkSize = 1001;

  struct Chunk {
    size_t offset = 0;
    size_t size = 0;
    uint8_t data[kMaxChunkSize];
    size_t data_size() { return 2 * sizeof(size_t) + size; }
  };

  static std::vector<uint8_t> generate_random_buffer(size_t size) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 255);
    std::vector<uint8_t> buffer(size);
    for (size_t i = 0; i < size; ++i) {
      buffer[i] = static_cast<uint8_t>(dist(gen));
    }
    return buffer;
  }

  // Create spans of random sizes within [min, max] that cover the entire `buffer` with no overlap.
  static std::vector<Chunk> random_chunks(size_t min, size_t max, std::span<uint8_t> buffer) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(min, max);

    std::vector<Chunk> chunks;
    size_t current_size = 0;

    while (current_size < buffer.size()) {
      size_t size = dist(gen);
      if ((current_size + size) > buffer.size()) {
        size = buffer.size() - current_size;
      }
      Chunk& chunk = chunks.emplace_back();
      chunk.offset = current_size;
      chunk.size = size;
      std::memcpy(chunk.data, reinterpret_cast<void*>(&buffer[current_size]), size);
      current_size += size;
    }
    return chunks;
  }

  BenchmarkVariableSize(size_t num_push_threads, size_t num_pop_threads)
      : num_push_threads_(num_push_threads),
        num_pop_threads_(num_pop_threads),
        num_unregistered_threads_(num_push_threads + num_pop_threads),
        push_result_("push", num_push_threads),
        pop_result_("pop", num_pop_threads) {
    send_buffer_ = generate_random_buffer(8 * 1024 * 1024);
    receive_buffer_.resize(send_buffer_.size(), 0);

    // Generate random chunks
    std::vector<Chunk> chunks = random_chunks(kMinChunkSize, kMaxChunkSize, send_buffer_);
    num_elements_to_push_ = chunks.size();
    std::cout << "Number of random chunks [" << kMinChunkSize << ", " << kMaxChunkSize
              << "]: " << chunks.size() << std::endl;
    chunks_per_thread_id_.resize(num_push_threads_);
    for (int i = 0; i < chunks.size(); ++i) {
      chunks_per_thread_id_[i % num_push_threads_].emplace_back(chunks[i]);
    }
    queue_ = std::make_unique<QueueT>();
  }

  void Run() {
    std::thread push_setup_thread(&BenchmarkVariableSize::LaunchPushThreads, this);
    std::thread pop_setup_thread(&BenchmarkVariableSize::LaunchPopThreads, this);
    push_setup_thread.join();
    pop_setup_thread.join();
    Print();

    std::string description = "Var sized queue";
    BenchmarkSummary& summary = BenchmarkStats::Get().benchmark_summaries[description];
    summary.description = description;
    summary.num_push_threads = num_push_threads_;
    summary.num_pop_threads = num_pop_threads_;
    summary.million_push_operations_per_second = push_result_.MillionOperationsPerSecond();
    summary.million_pop_operations_per_second = pop_result_.MillionOperationsPerSecond();
  }

  size_t GetNumPushedElements() const { return push_result_.TotalNumOperations(); }
  size_t GetNumPoppedElements() const { return pop_result_.TotalNumOperations(); }
  std::vector<uint8_t>& GetSendBuffer() { return send_buffer_; }
  std::vector<uint8_t>& GetReceiveBuffer() { return receive_buffer_; }
  const QueueT* GetQueue() const { return queue_.get(); }

 private:
  void LaunchPushThreads() {
    for (size_t i = 0; i < push_result_.results.size(); ++i) {
      push_result_.threads[i] =
          std::thread(&BenchmarkVariableSize::PushThread, this, i, &push_result_.results[i]);
    }
    BusyWaitForAllThreads();
    Timer timer(&push_result_.duration_ns);
    for (auto& thread : push_result_.threads) {
      thread.join();
    }
  }

  void LaunchPopThreads() {
    for (size_t i = 0; i < pop_result_.results.size(); ++i) {
      pop_result_.threads[i] =
          std::thread(&BenchmarkVariableSize::PopThread, this, i + 1, &pop_result_.results[i]);
    }
    Timer timer(&pop_result_.duration_ns);
    for (auto& thread : pop_result_.threads) {
      thread.join();
    }
  }

  void PushThread(size_t id, ThreadResult* result) {
    result->id = id;
    size_t push_per_thread = num_elements_to_push_ / push_result_.threads.size();
    RegisterAndBusyWaitForAllThreads();
    Timer timer(&result->duration_ns);
    std::vector<Chunk>& chunks = chunks_per_thread_id_[id];
    for (Chunk& chunk : chunks) {
      std::span<uint8_t> span{reinterpret_cast<uint8_t*>(&chunk), chunk.data_size()};
      while (!queue_->try_push(span))
        ;
      ++result->num_operations;
    }
  }

  void PopThread(size_t id, ThreadResult* result) {
    result->id = id;
    std::vector<uint8_t> buffer(sizeof(Chunk), 0);
    RegisterAndBusyWaitForAllThreads();
    Timer timer(&result->duration_ns);
    while (num_popped_elements_ < num_elements_to_push_) {
      if (queue_->try_pop(buffer)) {
        Chunk* chunk = reinterpret_cast<Chunk*>(buffer.data());
        std::memcpy(&receive_buffer_[chunk->offset], chunk->data, chunk->size);
        ++result->num_operations;
        ++num_popped_elements_;
      }
    }
  }

  void BusyWaitForAllThreads() {
    while (num_unregistered_threads_ > 0) {
    }
  }

  void RegisterAndBusyWaitForAllThreads() {
    --num_unregistered_threads_;
    BusyWaitForAllThreads();
  }

  void Print() {
    std::cout << "Send buffer == receive buffer? " << (send_buffer_ == receive_buffer_)
              << std::endl;
    // std::cout << StrFormat("Type: %s", queue_->description().c_str()) << std::endl;
    std::cout << StrFormat("Threads: %u push, %u pull\n", push_result_.size, pop_result_.size);
    std::cout << StrFormat("Push/Pop rates: %f/%f M/s\n", push_result_.MillionOperationsPerSecond(),
                           pop_result_.MillionOperationsPerSecond());
    push_result_.Print();
    pop_result_.Print();
    std::cout << std::endl;
  }

 private:
  std::unique_ptr<QueueT> queue_;
  std::span<const uint8_t> buffer_;
  std::vector<uint8_t> send_buffer_;
  std::vector<uint8_t> receive_buffer_;
  std::atomic<size_t> num_elements_to_push_;
  std::atomic<size_t> num_popped_elements_;
  std::atomic<size_t> num_unregistered_threads_;

  size_t num_push_threads_ = 0;
  size_t num_pop_threads_ = 0;
  std::vector<std::vector<Chunk>> chunks_per_thread_id_;

  Result push_result_;
  Result pop_result_;
};

}  // namespace sham