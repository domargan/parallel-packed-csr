//
// Created by menges on 7/7/20.
//

#include "../pcsr/PCSR.h"

#ifndef PPPCSR_H
#define PPPCSR_H

class PPPCSR {
 public:
  // data members
  edge_list_t edges;

  PPPCSR(uint32_t init_n, uint32_t, bool lock_search);
  //    PPPCSR(uint32_t init_n, vector<condition_variable*> *cvs, bool search_lock);
  //    ~PPPCSR();
  /** Public API */
  bool edge_exists(uint32_t src, uint32_t dest);
  void add_node();
  void add_edge(uint32_t src, uint32_t dest, uint32_t value);
  void remove_edge(uint32_t src, uint32_t dest);
  void read_neighbourhood(int src);

  std::size_t get_partiton(size_t vertex_id);

 private:
  /// different partitions
  std::vector<PCSR> partitions;

  /// start index vertices in the partitions
  std::vector<size_t> distribution;
};

#endif  // PPPCSR_H
