/**
 * @file thread_pool_pppcsr.h
 * @author Christian Menges
 */

#include <queue>
#include <thread>
#include <vector>

#include "../pppcsr/PPPCSR.h"
#include "../utility/concurrentqueue.h"
#include "task.h"

using namespace std;
#ifndef PPPCSR_THREAD_POOL_H
#define PPPCSR_THREAD_POOL_H

// enum class Operation { READ, ADD, DELETE };

class ThreadPoolPPPCSR {
 public:
  PPPCSR *pcsr;

  explicit ThreadPoolPPPCSR(const int NUM_OF_THREADS, bool lock_search, uint32_t init_num_nodes,
                            int partitions_per_domain, bool use_numa, bool balance);
  ~ThreadPoolPPPCSR() = default;
  /** Public API */
  void submit_add(int thread_id, int src, int dest);     // submit task to thread {thread_id} to insert edge {src, dest}
  void submit_delete(int thread_id, int src, int dest);  // submit task to thread {thread_id} to delete edge {src, dest}
  void submit_read(int, int);  // submit task to thread {thread_id} to read the neighbourhood of vertex {src}
  void start(int threads);     // start the threads
  void stop();                 // stop the threads

 private:
  vector<thread> thread_pool;
  vector<moodycamel::ConcurrentQueue<task>> tasks;
  size_t numberOfQueues;
  chrono::steady_clock::time_point s;
  chrono::steady_clock::time_point end;
  std::atomic_bool finished;

  template <bool isMasterThread>
  void execute(int);

  const int available_nodes;
  const bool balance;
  size_t queueTurn = 0;
  std::vector<unsigned> indeces;
  int partitions_per_domain = 1;
  std::vector<int> threadToDomain;
  std::vector<int> threadToQueue;
  std::vector<int> firstThreadDomain;
  std::vector<int> numThreadsDomain;
};

#endif  // PPPCSR_THREAD_POOL_H
