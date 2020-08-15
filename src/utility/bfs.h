/**
 * @file bfs.h
 * @author Christian Menges
 */

#include <cstdint>
#include <queue>
#include <vector>

using namespace std;

#ifndef PARALLEL_PACKED_CSR_BFS_H
#define PARALLEL_PACKED_CSR_BFS_H

template <typename T>
vector<uint32_t> bfs(T &graph, uint32_t start_node) {
  uint64_t n = graph.get_n();
  vector<uint32_t> out(n, UINT32_MAX);
  queue<uint32_t> next;
  next.push(start_node);
  out[start_node] = 0;

  while (!next.empty()) {
    uint32_t active = next.front();
    next.pop();

    // get neighbors
    for (const int neighbour : graph.get_neighbourhood(active)) {
      if (out[neighbour] == UINT32_MAX) {
        next.push(neighbour);
        out[neighbour] = out[active] + 1;
      }
    }
  }
  return out;
}

#endif  // PARALLEL_PACKED_CSR_BFS_H
