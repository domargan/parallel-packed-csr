/**
 * Created by Eleni Alevra
 * modified by Christian Menges
 */

#include <bfs.h>
#include <pagerank.h>

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

#include "thread_pool/thread_pool.h"
#include "thread_pool_pppcsr/thread_pool_pppcsr.h"

using namespace std;

// Reads edge list with comma separator
pair<vector<pair<int, int>>, int> read_input(string filename, char delimiter = ',') {
  ifstream f;
  string line;
  f.open(filename);
  if (!f.good()) {
    exit(EXIT_FAILURE);
  }
  vector<pair<int, int>> edges;
  int num_nodes = 0;
  while (getline(f, line)) {
    if (line.find(delimiter) == std::string::npos) {
      return read_input(filename, ' ');
    }
    int src = stoi(line.substr(0, line.find(delimiter)));
    num_nodes = std::max(num_nodes, src);
    int target = stoi(line.substr(line.find(delimiter) + 1, line.size()));
    num_nodes = std::max(num_nodes, target);
    edges.emplace_back(src, target);
  }
  return make_pair(edges, num_nodes);
}

// Does insertions
template <typename ThreadPool_t>
void update_existing_graph(const vector<pair<int, int>> &input, ThreadPool_t *thread_pool, int threads, int size) {
  for (int i = 0; i < size; i++) {
    thread_pool->submit_add(i % threads, input[i].first, input[i].second);
  }
  auto start = chrono::steady_clock::now();
  thread_pool->start(threads);
  thread_pool->stop();
  auto finish = chrono::steady_clock::now();
  //  cout << "Elapsed wall clock time: " << chrono::duration_cast<chrono::milliseconds>(finish - start).count() <<
  //  endl;
}

// Does deletions
template <typename ThreadPool_t>
void thread_pool_deletions(ThreadPool_t *thread_pool, const vector<pair<int, int>> &deletions, int threads, int size) {
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

template <typename ThreadPool_t>
void execute(int threads, int size, bool insert, const vector<pair<int, int>> &core_graph,
             const vector<pair<int, int>> &updates, std::unique_ptr<ThreadPool_t> &thread_pool) {
  // Load core graph
  update_existing_graph(core_graph, thread_pool.get(), threads, core_graph.size());
  // Do updates
  if (insert) {
    update_existing_graph(updates, thread_pool.get(), threads, size);
  } else {
    thread_pool_deletions(thread_pool.get(), updates, threads, size);
  }

  // run BFS
  {
    auto start = chrono::steady_clock::now();
    auto res = bfs(*thread_pool->pcsr, 0);
    auto finish = chrono::steady_clock::now();
    cout << "BFS result size: " << res.size() << endl;
    cout << "BFS time: " << chrono::duration_cast<chrono::milliseconds>(finish - start).count() << endl;
  }

  {
    vector<float> weights(thread_pool->pcsr->get_n(), 1.0f);
    auto start = chrono::steady_clock::now();
    auto res = pagerank(*thread_pool->pcsr, weights);
    auto finish = chrono::steady_clock::now();
    cout << "Pagerank time: " << chrono::duration_cast<chrono::milliseconds>(finish - start).count() << endl;
  }
}

enum class Version { PPCSR, PPPCSR, PPPCSRNUMA };

int main(int argc, char *argv[]) {
  int threads = 8;
  int size = 1000000;
  int num_nodes = 0;
  bool lock_search = true;
  bool insert = true;
  Version v = Version::PPPCSRNUMA;
  int partitions_per_domain = 1;
  vector<pair<int, int>> core_graph;
  vector<pair<int, int>> updates;
  for (int i = 1; i < argc; i++) {
    string s = string(argv[i]);
    if (s.rfind("-threads=", 0) == 0) {
      threads = stoi(s.substr(string("-threads=").length(), s.length()));
    } else if (s.rfind("-size=", 0) == 0) {
      size = stoi(s.substr(string("-size=").length(), s.length()));
    } else if (s.rfind("-lock_free", 0) == 0) {
      lock_search = false;
    } else if (s.rfind("-insert", 0) == 0) {
      insert = true;
    } else if (s.rfind("-delete", 0) == 0) {
      insert = false;
    } else if (s.rfind("-pppcsrnuma", 0) == 0) {
      v = Version::PPPCSRNUMA;
    } else if (s.rfind("-pppcsr", 0) == 0) {
      v = Version::PPPCSR;
    } else if (s.rfind("-ppcsr", 0) == 0) {
      v = Version::PPCSR;
    } else if (s.rfind("-partitions_per_domain=", 0) == 0) {
      partitions_per_domain = stoi(s.substr(string("-partitions_per_domain=").length(), s.length()));
    } else if (s.rfind("-core_graph=", 0) == 0) {
      string core_graph_filename = s.substr(string("-core_graph=").length(), s.length());
      int temp = 0;
      std::tie(core_graph, temp) = read_input(core_graph_filename);
      num_nodes = std::max(num_nodes, temp);
    } else if (s.rfind("-update_file=", 0) == 0) {
      string update_filename = s.substr(string("-update_file=").length(), s.length());
      cout << update_filename << endl;
      int temp = 0;
      std::tie(updates, temp) = read_input(update_filename);
      num_nodes = std::max(num_nodes, temp);
    }
  }
  if (core_graph.empty()) {
    cout << "Using default core graph" << endl;
    int temp = 0;
    std::tie(core_graph, temp) = read_input("../data/shuffled_higgs.txt");
    num_nodes = std::max(num_nodes, temp);
  }
  if (updates.empty()) {
    cout << "Using default update graph" << endl;
    int temp = 0;
    std::tie(updates, temp) = read_input("../data/shuffled_higgs.txt");
    num_nodes = std::max(num_nodes, temp);
  }
  cout << "Core graph size: " << core_graph.size() << endl;
  //   sort(core_graph.begin(), core_graph.end());
  switch (v) {
    case Version::PPCSR: {
      auto thread_pool = make_unique<ThreadPool>(threads, lock_search, num_nodes, partitions_per_domain);
      execute(threads, size, insert, core_graph, updates, thread_pool);
      break;
    }
    case Version::PPPCSR: {
      auto thread_pool = make_unique<ThreadPoolPPPCSR>(threads, lock_search, num_nodes, partitions_per_domain, false);
      execute(threads, size, insert, core_graph, updates, thread_pool);
      break;
    }
    default: {
      auto thread_pool = make_unique<ThreadPoolPPPCSR>(threads, lock_search, num_nodes, partitions_per_domain, true);
      execute(threads, size, insert, core_graph, updates, thread_pool);
    }
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

  return 0;
}
