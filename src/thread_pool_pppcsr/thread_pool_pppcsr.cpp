/**
 * @file thread_pool_pppcsr.cpp
 * @author Christian Menges
 */

#include "thread_pool_pppcsr.h"

#include <numa.h>

#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using namespace std;

/**
 * Initializes a pool of threads. Every thread has its own task queue.
 */
ThreadPoolPPPCSR::ThreadPoolPPPCSR(const int NUM_OF_THREADS, bool lock_search, uint32_t init_num_nodes,
                                   int partitions_per_domain, bool use_numa)
    : tasks(NUM_OF_THREADS),
      finished(false),
      available_nodes(std::min(numa_max_node() + 1, NUM_OF_THREADS)),
      indeces(available_nodes, 0),
      partitions_per_domain(partitions_per_domain),
      threadToDomain(NUM_OF_THREADS),
      firstThreadDomain(available_nodes, 0),
      numThreadsDomain(available_nodes) {
  pcsr = new PPPCSR(init_num_nodes, init_num_nodes, lock_search, available_nodes, partitions_per_domain, use_numa);

  int d = available_nodes;
  int minNumThreads = NUM_OF_THREADS / d;
  int threshold = NUM_OF_THREADS % d;
  int counter = 0;
  int currentDomain = 0;

  for (int i = 0; i < NUM_OF_THREADS; i++) {
    threadToDomain[i] = currentDomain;
    counter++;
    if (counter == minNumThreads + (currentDomain < threshold)) {
      numThreadsDomain[currentDomain] = counter;
      firstThreadDomain[currentDomain] = i - counter + 1;
      counter = 0;
      currentDomain++;
    }
  }
}

// Function executed by worker threads
// Does insertions, deletions and reads on the PCSR
// Finishes when finished is set to true and there are no outstanding tasks
template <bool isMasterThread>
void ThreadPoolPPPCSR::execute(const int thread_id) {
  cout << "Thread " << thread_id << " has " << tasks[thread_id].size() << " tasks, runs on domain "
       << threadToDomain[thread_id] << endl;
  if (numa_available() >= 0) {
    numa_run_on_node(threadToDomain[thread_id]);
  }
  int registered = -1;

  while (!tasks[thread_id].empty() || (!isMasterThread && !finished)) {
    if (!tasks[thread_id].empty()) {
      task t = tasks[thread_id].front();
      tasks[thread_id].pop();

      int currentPar = pcsr->get_partiton(t.src);

      if (registered != currentPar) {
        if (registered != -1) {
          pcsr->unregisterThread(registered);
        }
        pcsr->registerThread(currentPar);
        registered = currentPar;
      }
      if (t.add) {
        pcsr->add_edge(t.src, t.target, 1);
      } else if (!t.read) {
        pcsr->remove_edge(t.src, t.target);
      } else {
        pcsr->read_neighbourhood(t.src);
      }
    } else {
      if (registered != -1) {
        pcsr->unregisterThread(registered);
        registered = -1;
      }
    }
  }
  if (registered != -1) {
    pcsr->unregisterThread(registered);
  }
}

// Submit an update for edge {src, target} to thread with number thread_id
void ThreadPoolPPPCSR::submit_add(int thread_id, int src, int target) {
  (void)thread_id;
  auto par = pcsr->get_partiton(src) / partitions_per_domain;
  auto index = (indeces[par]++) % numThreadsDomain[par];
  tasks[firstThreadDomain[par] + index].push(task{true, false, src, target});
}

// Submit a delete edge task for edge {src, target} to thread with number thread_id
void ThreadPoolPPPCSR::submit_delete(int thread_id, int src, int target) {
  (void)thread_id;
  auto par = pcsr->get_partiton(src) / partitions_per_domain;
  auto index = (indeces[par]++) % numThreadsDomain[par];
  tasks[firstThreadDomain[par] + index].push(task{false, false, src, target});
}

// Submit a read neighbourhood task for vertex src to thread with number thread_id
void ThreadPoolPPPCSR::submit_read(int thread_id, int src) {
  (void)thread_id;
  auto par = pcsr->get_partiton(src) / partitions_per_domain;
  auto index = (indeces[par]++) % numThreadsDomain[par];
  tasks[firstThreadDomain[par] + index].push(task{false, true, src, src});
}

// starts a new number of threads
// number of threads is passed to the constructor
void ThreadPoolPPPCSR::start(int threads) {
  s = chrono::steady_clock::now();
  finished = false;

  for (int i = 1; i < threads; i++) {
    thread_pool.push_back(thread(&ThreadPoolPPPCSR::execute<false>, this, i));
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
  execute<true>(0);
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
  cout << "Elapsed wall clock time: " << chrono::duration_cast<chrono::milliseconds>(end - s).count() << endl;
  thread_pool.clear();
}
