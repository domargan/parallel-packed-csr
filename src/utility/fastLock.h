/**
 * @file fastLock.h
 * @author Christian Menges
 */

#ifndef PARALLEL_PACKED_CSR_FASTLOCK_H
#define PARALLEL_PACKED_CSR_FASTLOCK_H

#include <atomic>
#include <iostream>
#include <shared_mutex>
#include <thread>

class FastLock {
  using lock_type = std::shared_timed_mutex;

 public:
  FastLock() : lockRequested{false}, thread_counter{0}, arrived_threads{0} {}

  ~FastLock() = default;

  FastLock(const FastLock &) = delete;

  FastLock &operator=(const FastLock &) = delete;

  void lock() {
    arrived_threads.fetch_add(1);
    mtx.lock();
    lockRequested = true;
    while (arrived_threads.load() < thread_counter.load()) {
      std::this_thread::yield();
    }
    arrived_threads.fetch_sub(1);
  }

  void unlock() {
    lockRequested.store(false);
    mtx.unlock();
  }

  void registerThread() { thread_counter.fetch_add(1); }

  void unregisterThread() { thread_counter.fetch_sub(1); }

  void lock_shared() {
    if (lockRequested) {
      arrived_threads.fetch_add(1);
      std::shared_lock<lock_type> lck(mtx);
      arrived_threads.fetch_sub(1);
    }
  }

  void unlock_shared() { lock_shared(); }

  bool lockable() { return true; }

 private:
  lock_type mtx;
  std::atomic<bool> lockRequested;
  std::atomic<unsigned int> thread_counter;
  std::atomic<unsigned int> arrived_threads;
};

#endif  // PARALLEL_PACKED_CSR_FASTLOCK_H
