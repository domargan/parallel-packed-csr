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
#include <functional>
#include <map>

#include "thread_pool/thread_pool.h"
#include "thread_pool_pppcsr/thread_pool_pppcsr.h"

#include <benchmark/benchmark.h>

using namespace std;

// enum class Operation { READ, ADD, DELETE };

// Reads edge list with separator
pair<vector<tuple<Operation, int, int>>, int> read_input(string filename, Operation defaultOp) {
  ifstream f;
  string line;
  f.open(filename);
  if (!f.good()) {
    std::cerr << "Invalid file" << std::endl;
    exit(EXIT_FAILURE);
  }
  vector<tuple<Operation, int, int>> edges;
  int num_nodes = 0;
  std::size_t pos, pos2;
  while (getline(f, line)) {
    int src = stoi(line, &pos);
    int target = stoi(line.substr(pos + 1), &pos2);

    num_nodes = std::max(num_nodes, std::max(src, target));

    Operation op = defaultOp;
    if (pos + 1 + pos2 + 1 < line.length()) {
      switch (line[pos + 1 + pos2 + 1]) {
        case '1':
          op = Operation::ADD;
          break;
        case '0':
          op = Operation::DELETE;
          break;
        default:
          cerr << "Invalid operation";
      }
    }
    edges.emplace_back(op, src, target);
  }
  return make_pair(edges, num_nodes);
}

// Does insertions
template <typename ThreadPool_t>
void update_existing_graph(const vector<tuple<Operation, int, int>> &input, ThreadPool_t *thread_pool, int threads,
                           int size) {
  for (int i = 0; i < size; i++) {
    switch (get<0>(input[i])) {
      case Operation::ADD:
        thread_pool->submit_add(i % threads, get<1>(input[i]), get<2>(input[i]));
        break;
      case Operation::DELETE:
        thread_pool->submit_delete(i % threads, get<1>(input[i]), get<2>(input[i]));
        break;
      case Operation::READ:
        cerr << "Not implemented\n";
        break;
    }
  }
  thread_pool->start(threads);
  thread_pool->stop();
}

template <typename ThreadPool_t>
void execute(int threads, int size, const vector<tuple<Operation, int, int>> &core_graph,
             const vector<tuple<Operation, int, int>> &updates, std::unique_ptr<ThreadPool_t> &thread_pool) {

  
  // Load core graph
  update_existing_graph(core_graph, thread_pool.get(), threads, core_graph.size());
  // Do updates
  update_existing_graph(updates, thread_pool.get(), threads, size);

  //    DEBUGGING CODE
  //    Check that all edges are there and in sorted order
  //    for (int i = 0; i < core_graph.size(); i++) {
  //        if (!thread_pool->pcsr->edge_exists(std::get<1>(core_graph[i]),std::get<2>(core_graph[i]))) {
  //            cout << "Not there " <<  std::get<1>(core_graph[i]) << " " <<
  //                 std::get<2>(core_graph[i]) << endl;
  //        }
  //    }
  //    for (int i = 0; i < size; i++) {
  //        if (!thread_pool->pcsr->edge_exists(std::get<1>(updates[i]), std::get<2>(updates[i]))) {
  //            cout << "Update not there " << std::get<1>(updates[i]) << " " <<
  //                 std::get<2>(updates[i]) << endl;
  //        }
  //    }
}

enum class Version { PPCSR, PPPCSR, PPPCSRNUMA };

int main(int argc, char *argv[]) {
  int threads = 8;
  int size = 1000000;
  int num_nodes = 0;
  bool lock_search = true;
  bool insert = true;
  bool balance = true;
  Version v = Version::PPPCSRNUMA;
  int partitions_per_domain = 1;
  vector<tuple<Operation, int, int>> core_graph;
  vector<tuple<Operation, int, int>> updates;
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
    } else if (s.rfind("-balance", 0) == 0) {
      balance = true;
    } else if (s.rfind("-cluster", 0) == 0) {
      balance = false;
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
      std::tie(core_graph, temp) = read_input(core_graph_filename, Operation::ADD);
      num_nodes = std::max(num_nodes, temp);
    } else if (s.rfind("-update_file=", 0) == 0) {
      string update_filename = s.substr(string("-update_file=").length(), s.length());
      cout << update_filename << endl;
      int temp = 0;
      Operation defaultOp = Operation::ADD;
      if (!insert) {
        defaultOp = Operation::DELETE;
      }
      std::tie(updates, temp) = read_input(update_filename, defaultOp);
      num_nodes = std::max(num_nodes, temp);
      size = std::min((size_t)size, updates.size());
    }
  }
  if (core_graph.empty()) {
    cout << "Core graph file not specified" << endl;
    exit(EXIT_FAILURE);
  }
  if (updates.empty()) {
    cout << "Updates file not specified" << endl;
    exit(EXIT_FAILURE);
  }
  cout << "Core graph size: " << core_graph.size() << endl;
  //   sort(core_graph.begin(), core_graph.end());
  switch (v) {
    case Version::PPCSR: {
      auto thread_pool = make_unique<ThreadPool>(threads, lock_search, num_nodes + 1, partitions_per_domain);
      execute(threads, size, core_graph, updates, thread_pool);
      break;
    }
    case Version::PPPCSR: {
      auto thread_pool =
          make_unique<ThreadPoolPPPCSR>(threads, lock_search, num_nodes + 1, partitions_per_domain, false, balance);
      execute(threads, size, core_graph, updates, thread_pool);
      break;
    }
    default: {
      auto thread_pool =
          make_unique<ThreadPoolPPPCSR>(threads, lock_search, num_nodes + 1, partitions_per_domain, true, balance);
      execute(threads, size, core_graph, updates, thread_pool);
    }
  }

  return 0;
}