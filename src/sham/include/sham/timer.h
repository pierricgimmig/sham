#pragma once

#include <os/log.h>
#include <os/signpost.h>

#include <chrono>
#include <cstdint>
#include <iostream>  // needed for std::cout

namespace sham {

class Timer {
 public:
  explicit Timer(uint64_t* time_ns) : time_ns_(time_ns) {
    start_ = std::chrono::high_resolution_clock::now();
  }

  ~Timer() {
    auto end = std::chrono::high_resolution_clock::now();
    *time_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
  }

 private:
  uint64_t* time_ns_;
  std::chrono::time_point<std::chrono::high_resolution_clock> start_;
};

}  // namespace sham

inline os_log_t GetProfilingLog() {
  static os_log_t log = os_log_create("com.yourcompany.sham", "PointsOfInterest");
  return log;
}

#define PROFILING_SCOPE(name_literal)                                                            \
  struct _ProfilingScope_##__LINE__ {                                                            \
    os_log_t log_;                                                                               \
    os_signpost_id_t id_;                                                                        \
    _ProfilingScope_##__LINE__() : log_(GetProfilingLog()), id_(os_signpost_id_generate(log_)) { \
      os_signpost_interval_begin(log_, id_, name_literal, );                                     \
    }                                                                                            \
    ~_ProfilingScope_##__LINE__() { os_signpost_interval_end(log_, id_, name_literal); }         \
  } _profiling_scope_instance_##__LINE__
