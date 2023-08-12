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

#include "external/concurrentqueue/concurrentqueue.h"

namespace sham {

// Adapter for MoodyCamel's concurrentqueue used in tests and benchmarks.
// NOTE: concurrentqueue is not fixed size and has no empty() or size() methods.
template <typename ElementT>
struct ConcurrentQueueAdapter {
  ConcurrentQueueAdapter() {}
  inline void push(const ElementT& e) { queue_.enqueue(e); }
  inline void push(ElementT&& e) { queue_.enqueue(std::forward<ElementT>(e)); }
  inline bool try_push(ElementT& e) { return queue_.try_enqueue(e); }
  inline bool try_push(ElementT&& e) { return queue_.try_enqueue(std::forward<ElementT>(e)); }
  inline bool try_pop(ElementT& e) { return queue_.try_dequeue(e); }
  std::string description() { return "MoodyCamel concurrent queue"; }
  moodycamel::ConcurrentQueue<ElementT> queue_;
};
}  // namespace sham
