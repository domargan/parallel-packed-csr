/**
 * @file thread_pool_pppcsr.cpp
 * @author Christian Menges
 */

#include "thread_pool_pppcsr.h"

#include <numa.h>

#include <cmath>
#include <ctime>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using namespace std;

/**
 * Initializes a pool of threads. Every thread has its own task queue.
 */
ThreadPoolPPPCSR::ThreadPoolPPPCSR(const int NUM_OF_THREADS, bool lock_search, uint32_t init_num_nodes, int partitions_per_domain) :
        available_nodes(numa_max_node()+1),
        partitions_per_domain(partitions_per_domain)
{
  tasks.resize(NUM_OF_THREADS);
  indeces.resize(available_nodes * partitions_per_domain, 0);
  pcsr = new PPPCSR(init_num_nodes, init_num_nodes, lock_search, partitions_per_domain);
}

// Function executed by worker threads
// Does insertions, deletions and reads on the PCSR
// Finishes when finished is set to true and there are no outstanding tasks
void ThreadPoolPPPCSR::execute(int thread_id) {
  cout << "Thread " << thread_id << " has " << tasks[thread_id].size() << " tasks, runs on domain " << (thread_id ) / (tasks.size() / available_nodes) << endl;
  if (numa_available() >= 0) {
    numa_run_on_node((thread_id ) / (tasks.size() / available_nodes));
  }
  while (!finished || !tasks[thread_id].empty()) {
    if (!tasks[thread_id].empty()) {
      task t = tasks[thread_id].front();
      tasks[thread_id].pop();
      if (t.add) {
        pcsr->add_edge(t.src, t.target, 1);
      } else if (!t.read) {
        pcsr->remove_edge(t.src, t.target);
      } else {
        pcsr->read_neighbourhood(t.src);
      }
    }
  }
}

// Submit an update for edge {src, target} to thread with number thread_id
void ThreadPoolPPPCSR::submit_add(int thread_id, int src, int target) {
  auto par = pcsr->get_partiton(src);
  auto index = (indeces[par]++)%(tasks.size() / available_nodes);
    tasks[(par / partitions_per_domain) * (tasks.size() / available_nodes) + index]
    .push(task{true, false, src, target});
}

// Submit a delete edge task for edge {src, target} to thread with number thread_id
void ThreadPoolPPPCSR::submit_delete(int thread_id, int src, int target) {
    auto par = pcsr->get_partiton(src);
    auto index = (indeces[par]++)%(tasks.size() / available_nodes);
    tasks[(par / partitions_per_domain) * (tasks.size() / available_nodes) + index]
            .push(task{false, false, src, target});
}

// Submit a read neighbourhood task for vertex src to thread with number thread_id
void ThreadPoolPPPCSR::submit_read(int thread_id, int src) {
    auto par = pcsr->get_partiton(src);
    auto index = (indeces[par]++) % (tasks.size() / available_nodes);
    tasks[(par / partitions_per_domain) * (tasks.size() / available_nodes) + index]
            .push(task{false, true, src, src});
}

// starts a new number of threads
// number of threads is passed to the constructor
void ThreadPoolPPPCSR::start(int threads) {
  s = chrono::steady_clock::now();
  finished = false;

  for (int i = 0; i < threads; i++) {
    thread_pool.push_back(thread(&ThreadPoolPPPCSR::execute, this, i));
    // Pin thread to core
    //    cpu_set_t cpuset;
    //    CPU_ZERO(&cpuset);
    //    CPU_SET((i * 4), &cpuset);
    //    if (i >= 4) {
    //      CPU_SET(1 + (i * 4), &cpuset);
    //    } else {
    //      CPU_SET(i * 4, &cpuset);
    //    }
    //    int rc = pthread_setaffinity_np(thread_pool.back().native_handle(),
    //                                    sizeof(cpu_set_t), &cpuset);
    //    if (rc != 0) {
    //      cout << "error pinning thread" << endl;
    //    }
  }
}

// Stops currently running worker threads without redistributing worker threads
// start() can still be used after this is called to start a new set of threads operating on the same pcsr
void ThreadPoolPPPCSR::stop() {
  finished = true;
  for (auto &&t : thread_pool) {
    if (t.joinable()) t.join();
    cout << "Done" << endl;
  }
  end = chrono::steady_clock::now();
  cout << "Elapsed wall clock time: " << chrono::duration_cast<chrono::microseconds>(end - s).count() << endl;
  thread_pool.clear();
}
