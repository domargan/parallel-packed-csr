#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

//#include "thread_pool/thread_pool.h"

#include "thread_pool_pppcsr/thread_pool_pppcsr.h"

using ThreadPool = ThreadPoolPPPCSR;

using namespace std;

// Reads edge list with comma separator
vector<pair<int, int>> read_input(string filename, char delimiter = ',') {
  ifstream f;
  string line;
  f.open(filename);
  if (!f.good()) {
    exit(EXIT_FAILURE);
  }
  vector<pair<int, int>> edges;
  while (getline(f, line)) {
    if (line.find(delimiter) == std::string::npos) {
      return read_input(filename, ' ');
    }
    int src = stoi(line.substr(0, line.find(delimiter)));
    int target = stoi(line.substr(line.find(delimiter) + 1, line.size()));
    edges.emplace_back(src, target);
  }
  return edges;
}

// Loads core graph
ThreadPool *insert_with_thread_pool(const vector<pair<int, int>> &input, int threads, bool lock_search) {
  int NUM_OF_THREADS = threads;
  if (threads < 8) {
    NUM_OF_THREADS = 8;
  }
  ThreadPool *thread_pool = new ThreadPool(NUM_OF_THREADS, lock_search);
  for (int i = 0; i < input.size(); i++) {
    thread_pool->submit_add(i % NUM_OF_THREADS, input[i].first, input[i].second);
  }
  auto start = chrono::steady_clock::now();
  thread_pool->start(NUM_OF_THREADS);
  thread_pool->stop();
  auto finish = chrono::steady_clock::now();
  //  cout << "Elapsed wall clock time: " << chrono::duration_cast<chrono::milliseconds>(finish - start).count() <<
  //  endl;
  return thread_pool;
}

// Does insertions
ThreadPool *update_existing_graph(const vector<pair<int, int>> &input, ThreadPool *thread_pool, int threads, int size) {
  for (int i = 0; i < size; i++) {
    thread_pool->submit_add(i % threads, input[i].first, input[i].second);
  }
  auto start = chrono::steady_clock::now();
  thread_pool->start(threads);
  thread_pool->stop();
  auto finish = chrono::steady_clock::now();
  //  cout << "Elapsed wall clock time: " << chrono::duration_cast<chrono::milliseconds>(finish - start).count() <<
  //  endl;
  return thread_pool;
}

// Does deletions
void thread_pool_deletions(ThreadPool *thread_pool, const vector<pair<int, int>> &deletions, int threads, int size) {
  int NUM_OF_THREADS = threads;
  for (int i = 0; i < size; i++) {
    thread_pool->submit_delete(i % NUM_OF_THREADS, deletions[i].first, deletions[i].second);
  }
  auto start = chrono::steady_clock::now();
  thread_pool->start(threads);
  thread_pool->stop();
  auto finish = chrono::steady_clock::now();
  //  cout << "Elapsed wall clock time: " << chrono::duration_cast<chrono::milliseconds>(finish - start).count() <<
  //  endl;
}

int main(int argc, char *argv[]) {
  int threads = 8;
  int size = 1000000;
  bool lock_search = true;
  bool insert = true;
  vector<pair<int, int>> core_graph;
  vector<pair<int, int>> updates;
  for (int i = 1; i < argc; i++) {
    string s = string(argv[i]);
    if (s.rfind("-threads=", 0) == 0) {
      threads = stoi(s.substr(9, s.length()));
    } else if (s.rfind("-size=", 0) == 0) {
      size = stoi(s.substr(6, s.length()));
    } else if (s.rfind("-lock_free", 0) == 0) {
      lock_search = false;
    } else if (s.rfind("-insert", 0) == 0) {
      insert = true;
    } else if (s.rfind("-delete", 0) == 0) {
      insert = false;
    } else if (s.rfind("-core_graph=", 0) == 0) {
      string core_graph_filename = s.substr(12, s.length());
      core_graph = read_input(core_graph_filename);
    } else if (s.rfind("-update_file=", 0) == 0) {
      string update_filename = s.substr(13, s.length());
      cout << update_filename << endl;
      updates = read_input(update_filename);
    }
  }
  if (core_graph.empty()) {
    cout << "Using default core graph" << endl;
    core_graph = read_input("shuffled_higgs.txt");
  }
  if (updates.empty()) {
    cout << "Using default update graph" << endl;
    updates = read_input("shuffled_higgs.txt");
  }
  cout << "Core graph size: " << core_graph.size() << endl;
  //   sort(core_graph.begin(), core_graph.end());
  // Load core graph
  ThreadPool *thread_pool = insert_with_thread_pool(core_graph, threads, lock_search);
  // Do updates
  if (insert) {
    update_existing_graph(updates, thread_pool, threads, size);
  } else {
    thread_pool_deletions(thread_pool, updates, threads, size);
  }

  // DEBUGGING CODE
  // Check that all edges are there and in sorted order
  //     for (int i = 0; i < core_graph.size(); i++) {
  //       if (!thread_pool->pcsr->edge_exists(core_graph[i].first, core_graph[i].second)) {
  //         cout << "Not there " << core_graph[i].first << " " << core_graph[i].second << endl;
  //       }
  //     }
  //        for (int i = 0; i < size; i++) {
  //          if (!thread_pool->pcsr->edge_exists(updates[i].first, updates[i].second)) {
  //            cout << "Not there" << endl;
  //          }
  //        }
  //     if (!thread_pool->pcsr->is_sorted()) {
  //       cout << "Not sorted" << endl;
  //     }
  delete thread_pool;

  return 0;
}
