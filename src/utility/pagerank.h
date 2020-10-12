/**
 * @file pagerank.h
 * @author Christian Menges
 */

#include <cstdint>
#include <queue>
#include <vector>

using namespace std;

#ifndef PARALLEL_PACKED_CSR_PAGERANK_H
#define PARALLEL_PACKED_CSR_PAGERANK_H

template <typename T, typename weight_t>
vector<weight_t> pagerank(T &graph, std::vector<weight_t> const &node_values) {
  auto n = graph.get_n();

  vector<weight_t> output(n, 0);
  for (uint64_t i = 0; i < n; i++) {
    const weight_t contrib = (node_values[i] / graph.getNode(i).num_neighbors);

    // get neighbors
    for (const int neighbour : graph.get_neighbourhood(i)) {
      output[neighbour] += contrib;
    }
  }
  return output;
}

#endif  // PARALLEL_PACKED_CSR_PAGERANK_H
