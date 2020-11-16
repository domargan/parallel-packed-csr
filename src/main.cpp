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
  thread_pool->submit_bulk_update(input, size, threads);
  thread_pool->start(threads);
  thread_pool->stop();
}

template <typename ThreadPool_t>
void execute(benchmark::State& state, int threads, int size, const vector<tuple<Operation, int, int>> &core_graph,
             const vector<tuple<Operation, int, int>> &updates, std::unique_ptr<ThreadPool_t> &thread_pool) {

  
  // Load core graph
  update_existing_graph(core_graph, thread_pool.get(), threads, core_graph.size());
  for(auto _ : state){
  // Do updates
  update_existing_graph(updates, thread_pool.get(), threads, size);
  }

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

// int main(int argc, char *argv[]) {
//   int threads = 8;
//   int size = 1000000;
//   int num_nodes = 0;
//   bool lock_search = true;
//   bool insert = true;
//   Version v = Version::PPPCSRNUMA;
//   int partitions_per_domain = 1;
//   vector<tuple<Operation, int, int>> core_graph;
//   vector<tuple<Operation, int, int>> updates;
//   for (int i = 1; i < argc; i++) {
//     string s = string(argv[i]);
    
//     } else if (s.rfind("-pppcsrnuma", 0) == 0) {
//       v = Version::PPPCSRNUMA;
//     } else if (s.rfind("-pppcsr", 0) == 0) {
//       v = Version::PPPCSR;
//     } else if (s.rfind("-ppcsr", 0) == 0) {
//       v = Version::PPCSR;
//     } else if (s.rfind("-partitions_per_domain=", 0) == 0) {
//       partitions_per_domain = stoi(s.substr(string("-partitions_per_domain=").length(), s.length()));
//     } else if (s.rfind("-core_graph=", 0) == 0) {
//       string core_graph_filename = s.substr(string("-core_graph=").length(), s.length());
//       int temp = 0;
//       std::tie(core_graph, temp) = read_input(core_graph_filename, Operation::ADD);
//       num_nodes = std::max(num_nodes, temp);
//     } else if (s.rfind("-update_file=", 0) == 0) {
//       string update_filename = s.substr(string("-update_file=").length(), s.length());
//       // cout << update_filename << endl;
//       int temp = 0;
//       Operation defaultOp = Operation::ADD;
//       if (!insert) {
//         defaultOp = Operation::DELETE;
//       }
//       std::tie(updates, temp) = read_input(update_filename, defaultOp);
//       num_nodes = std::max(num_nodes, temp);
//       size = std::min((size_t)size, updates.size());
//     }
//   }
//   if (core_graph.empty()) {
//     cout << "Core graph file not specified" << endl;
//     exit(EXIT_FAILURE);
//   }
//   if (updates.empty()) {
//     cout << "Updates file not specified" << endl;
//     exit(EXIT_FAILURE);
//   }
//   // cout << "Core graph size: " << core_graph.size() << endl;
//   //   sort(core_graph.begin(), core_graph.end());
//   switch (v) {
//     case Version::PPCSR: {
//       auto thread_pool = make_unique<ThreadPool>(threads, lock_search, num_nodes + 1, partitions_per_domain);
//       execute(threads, size, core_graph, updates, thread_pool);
//       break;
//     }
//     case Version::PPPCSR: {
//       auto thread_pool =
//           make_unique<ThreadPoolPPPCSR>(threads, lock_search, num_nodes + 1, partitions_per_domain, false);
//       execute(threads, size, core_graph, updates, thread_pool);
//       break;
//     }
//     default: {
//       auto thread_pool =
//           make_unique<ThreadPoolPPPCSR>(threads, lock_search, num_nodes + 1, partitions_per_domain, true);
//       execute(threads, size, core_graph, updates, thread_pool);
//     }
//   }

//   return 0;
// }

template <typename ThreadPoolType, const char* version, const char* UpdateType> static void Benchmark_PPPCSR(benchmark::State &state) {
  int threads = 8;
  int size = 1000000;
  int num_nodes = 0;
  bool lock_search = true;
  int partitions_per_domain = 1;
  vector<tuple<Operation, int, int>> core_graph;
  vector<tuple<Operation, int, int>> updates;

  auto insert = map<string, bool>({{"INSERT", true}, {"DELETE", false}}).at(UpdateType);

  Operation updateType = insert ? Operation::ADD : Operation::DELETE;

  string core_graph_filename = "/home/ak10318/PPCSR/parallel-packed-csr/data/shuffled_higgs.txt";
  int temp = 0;
  std::tie(core_graph, temp) = read_input(core_graph_filename, Operation::ADD);
  num_nodes = std::max(num_nodes, temp);
  string update_filename = "/home/ak10318/PPCSR/parallel-packed-csr/data/update_files/insertions.txt";
  std::tie(updates, temp) = read_input(update_filename, updateType);
  num_nodes = std::max(num_nodes, temp);
  size = std::min((size_t)size, updates.size());

  if (core_graph.empty()) {
    cout << "Core graph file not specified" << endl;
    exit(EXIT_FAILURE);
  }
  if (updates.empty()) {
    cout << "Updates file not specified" << endl;
    exit(EXIT_FAILURE);
  }

  auto thread_pool = make_unique<ThreadPoolType>(threads, lock_search, num_nodes + 1, partitions_per_domain, true);

  map<string, function<void(
                benchmark::State &, 
                int, 
                int, 
                const vector<tuple<Operation, int, int>>,
                const vector<tuple<Operation, int, int>>,
                std::unique_ptr<ThreadPoolType> &)
      >>(
    {
      {"PPPCSRNUMA", execute<ThreadPoolType>},
      // {Version::PPPCSR,},
      // {Version::PPCSR,}
    }
  ).at(version)(
          state, 
          threads, 
          size, 
          core_graph, 
          updates, 
          thread_pool
        );
}

static const char INSERT[] = "INSERT";
static const char DELETE[] = "DELETE";

static const char PPPCSRNUMA[] = "PPPCSRNUMA";

BENCHMARK_TEMPLATE(Benchmark_PPPCSR, ThreadPoolPPPCSR, PPPCSRNUMA, INSERT)->Unit(benchmark::kMillisecond);
// BENCHMARK_TEMPLATE(Benchmark_PPPCSR, "PPPCSRNUMA", ThreadPoolPPPCSR, false)->Unit(benchmark::kMillisecond);
// BENCHMARK_TEMPLATE(Benchmark, Version::PPPCSR, true)->Unit(benchmark::kMillisecond);
// BENCHMARK_TEMPLATE(Benchmark, Version::PPPCSR, false)->Unit(benchmark::kMillisecond);
// BENCHMARK_TEMPLATE(Benchmark, Version::PPCSR, true)->Unit(benchmark::kMillisecond);
// BENCHMARK_TEMPLATE(Benchmark, Version::PPCSR, false)->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
