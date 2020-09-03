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
 public:
  FastLock() : lockRequested{false}, thread_counter{0}, arrived_threads{0} { std::cout << "Lock init\n"; }

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

  inline void lock_shared() {
    if (lockRequested) {
      arrived_threads.fetch_add(1);
      mtx.lock_shared();
      arrived_threads.fetch_sub(1);
      mtx.unlock_shared();
    }
  }

  inline void unlock_shared() {
    if (lockRequested) {
      arrived_threads.fetch_add(1);
      mtx.lock_shared();
      mtx.unlock_shared();
    }
  }

  bool lockable() { return true; }

 private:
  std::shared_timed_mutex mtx;
  std::atomic<bool> lockRequested;
  std::atomic<int> thread_counter;
  std::atomic<int> arrived_threads;
};

#endif  // PARALLEL_PACKED_CSR_FASTLOCK_H
