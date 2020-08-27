/**
 * @file thread_pool_pppcsr.h
 * @author Christian Menges
 */

#include <queue>
#include <thread>
#include <vector>

#include "../pppcsr/PPPCSR.h"
using namespace std;
#ifndef PPPCSR_THREAD_POOL_H
#define PPPCSR_THREAD_POOL_H

/** Struct for tasks to the threads */
struct task {
  bool add;    // True if this is an add task. If this is false it means it's a delete.
  bool read;   // True if this is a read task.
  int src;     // Source vertex for this task's edge
  int target;  // Target vertex for this task's edge
};

class ThreadPoolPPPCSR {
 public:
  PPPCSR *pcsr;

  explicit ThreadPoolPPPCSR(const int NUM_OF_THREADS, bool lock_search, uint32_t init_num_nodes,
                            int partitions_per_domain);
  ~ThreadPoolPPPCSR() = default;
  /** Public API */
  void submit_add(int thread_id, int src, int dest);     // submit task to thread {thread_id} to insert edge {src, dest}
  void submit_delete(int thread_id, int src, int dest);  // submit task to thread {thread_id} to delete edge {src, dest}
  void submit_read(int, int);  // submit task to thread {thread_id} to read the neighbourhood of vertex {src}
  void start(int threads);     // start the threads
  void stop();                 // stop the threads

 private:
  vector<thread> thread_pool;
  vector<queue<task>> tasks;
  chrono::steady_clock::time_point s;
  chrono::steady_clock::time_point end;
  std::atomic_bool finished;

  void execute(int);

  const int available_nodes;
  std::vector<unsigned> indeces;
  int partitions_per_domain = 1;
};

#endif  // PPPCSR_THREAD_POOL_H
