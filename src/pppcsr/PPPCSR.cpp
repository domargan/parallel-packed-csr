//
// Created by menges on 7/7/20.
//

#include "PPPCSR.h"

#include <numa.h>

#include <cmath>
#include <iostream>

PPPCSR::PPPCSR(uint32_t init_n, uint32_t src_n, bool lock_search) {
  std::size_t numPartitions = 1;
  if (numa_available() < 0) {
    std::cout << "NUMA not available. Using single partition\n";
  } else {
    numPartitions = numa_max_node() + 2;
  }
  partitions.reserve(numPartitions);
  distribution.reserve(numPartitions);
  distribution.push_back(0);
  size_t partitionSize = std::ceil(init_n / numPartitions);
  for (std::size_t i = 0; i < numPartitions; i++) {
    if (i == numPartitions - 1) {
      partitionSize = init_n - i * partitionSize;
    }
    partitions.emplace_back(partitionSize, partitionSize, lock_search, 0);
    if (i > 0) {
      distribution.push_back(distribution.back() + partitionSize);
    }
  }
}

bool PPPCSR::edge_exists(uint32_t src, uint32_t dest) {
  return partitions[get_partiton(src)].edge_exists(src - distribution[get_partiton(src)], dest);
}

void PPPCSR::add_node() { partitions.back().add_node(); }

void PPPCSR::add_edge(uint32_t src, uint32_t dest, uint32_t value) {
  partitions[get_partiton(src)].add_edge(src - distribution[get_partiton(src)], dest, value);
}

void PPPCSR::remove_edge(uint32_t src, uint32_t dest) {
  partitions[get_partiton(src)].remove_edge(src - distribution[get_partiton(src)], dest);
}

void PPPCSR::read_neighbourhood(int src) {
  partitions[get_partiton(src)].read_neighbourhood(src - distribution[get_partiton(src)]);
}

std::size_t PPPCSR::get_partiton(size_t vertex_id) {
  for (std::size_t i = 1; i < distribution.size(); i++) {
    if (distribution[i] > vertex_id) {
      return i - 1;
    }
  }
  // Return last partition
  return distribution.size() - 1;
}
