/**
 * @file PPPCSR.cpp
 * @author Christian Menges
 */

#include "PPPCSR.h"

#include <numa.h>

#include <cmath>
#include <iostream>

PPPCSR::PPPCSR(uint32_t init_n, uint32_t src_n, bool lock_search, int numDomain, int partitionsPerDomain, bool use_numa)
    : partitionsPerDomain(partitionsPerDomain) {
  std::size_t numDomains = numDomain;

  partitions.reserve(numDomains * partitionsPerDomain);
  distribution.reserve(numDomains * partitionsPerDomain);
  distribution.push_back(0);
  size_t partitionSize = std::ceil(init_n / (numDomains * partitionsPerDomain));

  for (std::size_t i = 0; i < numDomains; i++) {
    for (std::size_t p = 0; p < partitionsPerDomain; p++) {
      if (i > 0 || p > 0) {
        distribution.push_back(distribution.back() + partitionSize);
      }
      if (i == numDomains - 1 && p == partitionsPerDomain - 1) {
        partitionSize = init_n - ((i * partitionsPerDomain) + p) * partitionSize;
      }
      partitions.emplace_back(partitionSize, partitionSize, lock_search, (use_numa) ? i : -1);
    }
  }
  cout << "Number of partitions: " << partitions.size() << std::endl;
}

bool PPPCSR::edge_exists(uint32_t src, uint32_t dest) {
  return partitions[get_partiton(src)].edge_exists(src - distribution[get_partiton(src)], dest);
}

vector<int> PPPCSR::get_neighbourhood(int src) const {
  return partitions[get_partiton(src)].get_neighbourhood(src - distribution[get_partiton(src)]);
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

std::size_t PPPCSR::get_partiton(size_t vertex_id) const {
  for (std::size_t i = 1; i < distribution.size(); i++) {
    if (distribution[i] > vertex_id) {
      return i - 1;
    }
  }
  // Return last partition
  return distribution.size() - 1;
}

uint64_t PPPCSR::get_n() {
  uint64_t n = 0;
  for (int i = 0; i < partitions.size(); i++) {
    n += partitions[i].get_n();
  }
  return n;
}

node_t &PPPCSR::getNode(int id) { return partitions[get_partiton(id)].getNode(id - distribution[get_partiton(id)]); }

const node_t &PPPCSR::getNode(int id) const {
  return partitions[get_partiton(id)].getNode(id - distribution[get_partiton(id)]);
}
