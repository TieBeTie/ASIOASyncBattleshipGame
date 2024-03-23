#pragma once

#include "Common.h"

namespace net {
template <typename T>
class TSDeque {
 public:
  TSDeque() = default;
  // нельзя копировать, поэтому
  TSDeque(const TSDeque<T>&) = delete;
  ~TSDeque() { Clear(); }

 public:
  const T& Front() {
    std::scoped_lock lock(deque_mutex_);
    return deque_.front();
  }

  const T& Back() {
    std::scoped_lock lock(deque_mutex_);
    return deque_.back();
  }

  T PopFront() {
    std::scoped_lock lock(deque_mutex_);
    auto t = std::move(deque_.front());
    deque_.pop_front();
    return t;
  }

  T PopBack() {
    std::scoped_lock lock(deque_mutex_);
    auto t = std::move(deque_.back());
    deque_.pop_back();
    return t;
  }

  void PushBack(const T& item) {
    std::scoped_lock lock(deque_mutex_);
    deque_.emplace_back(std::move(item));

    std::unique_lock<std::mutex> ulock(wait_push_mutex_);
    is_pushed_.notify_one();
  }

  void PushFront(const T& item) {
    std::scoped_lock lock(deque_mutex_);
    deque_.emplace_front(std::move(item));
    std::unique_lock<std::mutex> ulock(wait_push_mutex_);
    is_pushed_.notify_one();
  }

  bool Empty() {
    std::scoped_lock lock(deque_mutex_);
    return deque_.empty();
  }


  size_t Size() {
    std::scoped_lock lock(deque_mutex_);
    return deque_.size();
  }

  void Clear() {
    std::scoped_lock lock(deque_mutex_);
    deque_.clear();
  }

  void Wait() {
    while (Empty()) {
      std::unique_lock<std::mutex> ulock(wait_push_mutex_);
      is_pushed_.wait(ulock);
    }
  }

 protected:
  std::mutex deque_mutex_;
  std::deque<T> deque_;
  std::condition_variable is_pushed_;
  std::mutex wait_push_mutex_;
};
} 
