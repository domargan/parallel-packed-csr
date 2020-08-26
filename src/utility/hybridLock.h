/**
 * @file hybridLock.h
 * @author Christian Menges
 */

#ifndef PARALLEL_PACKED_CSR_HYBRIDLOCK_H
#define PARALLEL_PACKED_CSR_HYBRIDLOCK_H

#include <atomic>
#include <shared_mutex>

class HybridLock {
 public:
  HybridLock() : version_counter{0} {}
  ~HybridLock() = default;

  HybridLock(const HybridLock &) = delete;
  HybridLock &operator=(const HybridLock &) = delete;

  inline HybridLock &operator++() {
    version_counter++;
    return *this;
  }

  inline HybridLock &operator--() {
    version_counter--;
    return *this;
  }

  inline void lock() { mtx.lock(); }
  inline void unlock() { mtx.unlock(); }

  inline void lock_shared() { mtx.lock_shared(); }
  inline void unlock_shared() { mtx.unlock_shared(); }

  inline int load() const { return version_counter.load(); }

  bool lockable() {
    auto r = mtx.try_lock();
    if (r) {
      mtx.unlock();
    }
    return r;
  }

 private:
  std::shared_timed_mutex mtx;
  std::atomic<int> version_counter;
};

#endif  // PARALLEL_PACKED_CSR_HYBRIDLOCK_H
