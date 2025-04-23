//! Not file used (changed for io async methods)

#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename tv>
class thread_queue {
public:
  void push(const tv& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(value);
    cond_var_.notify_one();
  }

  bool pop(tv& value) {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this] { return !queue_.empty(); });
    value = queue_.front();
    queue_.pop();
    return true;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.emtpy();
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable cond_var_;
  std::queue<tv> queue_;
};