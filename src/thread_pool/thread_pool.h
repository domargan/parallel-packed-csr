/**
 * Created by Eleni Alevra on 02/06/2020.
 * modified by Christian Menges
 */

#include <queue>
#include <thread>
#include <vector>

#include "../pcsr/PCSR.h"
using namespace std;
#ifndef PCSR2_THREAD_POOL_H
#define PCSR2_THREAD_POOL_H

/** Struct for tasks to the threads */
struct task {
  bool add;    // True if this is an add task. If this is false it means it's a delete.
  bool read;   // True if this is a read task.
  int src;     // Source vertex for this task's edge
  int target;  // Target vertex for this task's edge
};

class ThreadPool {
 public:
  PCSR *pcsr;

  explicit ThreadPool(const int NUM_OF_THREADS, bool lock_search, uint32_t init_num_nodes);
  ~ThreadPool() {}
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
  bool finished = false;

  void execute(int);
};

#endif  // PCSR2_THREAD_POOL_H
