/**
 * @file PPPCSR.h
 * @author Christian Menges
 */

#include "../pcsr/PCSR.h"

#ifndef PPPCSR_H
#define PPPCSR_H

class PPPCSR {
 public:
  // data members
  edge_list_t edges;

  PPPCSR(uint32_t init_n, uint32_t, bool lock_search, int numDomain, int partitionsPerDomain, bool use_numa);
  //    PPPCSR(uint32_t init_n, vector<condition_variable*> *cvs, bool search_lock);
  //    ~PPPCSR();
  /** Public API */
  bool edge_exists(uint32_t src, uint32_t dest);
  void add_node();
  void add_edge(uint32_t src, uint32_t dest, uint32_t value);
  void remove_edge(uint32_t src, uint32_t dest);
  void read_neighbourhood(int src);

  std::size_t get_partiton(size_t vertex_id) const;

  vector<int> get_neighbourhood(int src) const;

  /**
   * Returns the node count
   * @return node count
   */
  uint64_t get_n();

  /**
   * Returns a ref. to the node with the given id
   * @return ref. to node
   */
  node_t &getNode(int id);

  /**
   * Returns a const ref. to the node with the given id
   * @return const ref. to node
   */
  const node_t &getNode(int id) const;

  void registerThread(int par) { partitions[par].edges.global_lock->registerThread(); }

  void unregisterThread(int par) { partitions[par].edges.global_lock->unregisterThread(); }

 private:
  /// different partitions
  std::vector<PCSR> partitions;

  /// start index vertices in the partitions
  std::vector<size_t> distribution;

  int partitionsPerDomain;
};

#endif  // PPPCSR_H
