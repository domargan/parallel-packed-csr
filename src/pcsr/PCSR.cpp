/**
 * This file was cloned from https://github.com/wheatman/Packed-Compressed-Sparse-Row/. The
 * parts of the code that Eleni Alevra has added for the parallel version are marked by comments.
 * modified by Christian Menges
 */
#include "PCSR.h"

#include <numa.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <queue>
#include <tuple>
#include <vector>

using namespace std;

// find index of first 1-bit (least significant bit)
static inline int bsf_word(int word) {
  int result;
  __asm__ volatile("bsf %1, %0" : "=r"(result) : "r"(word));
  return result;
}

static inline int bsr_word(int word) {
  int result;
  __asm__ volatile("bsr %1, %0" : "=r"(result) : "r"(word));
  return result;
}

typedef struct _pair_double {
  double x;
  double y;
} pair_double;

void PCSR::nodes_unlock_shared(bool unlock, int start_node, int end_node) {
  if (unlock) {
    for (int i = start_node; i <= end_node; i++) {
      edges.node_locks[i]->unlock_shared();
    }
  }
}

template <typename T>
void checkAllocation(T *ptr) {
  if (ptr == NULL) {
    cout << "Allocation failed. Abort\n";
    exit(EXIT_FAILURE);
  }
}

// same as find_leaf, but does it for any level in the tree
// index: index in array
// len: length of sub-level.
int find_node(int index, int len) { return (index / len) * len; }

// null overrides sentinel
// e.g. in rebalance, we check if an edge is null
// first before copying it into temp, then fix the sentinels.
bool is_sentinel(edge_t e) { return e.dest == UINT32_MAX || e.value == UINT32_MAX; }

// bool is_null(edge_t e) { return e.value == 0; }

void PCSR::clear() {
  int n = 0;
  if (is_numa_available) {
    numa_free(edges.items, edges.N * sizeof(*(edges.items)));
  } else {
    free(edges.items);
  }
  edges.N = 2 << bsr_word(n);
  edges.logN = (1 << bsr_word(bsr_word(edges.N) + 1));
  edges.H = bsr_word(edges.N / edges.logN);
}

vector<tuple<uint32_t, uint32_t, uint32_t>> PCSR::get_edges() {
  uint64_t n = get_n();
  vector<tuple<uint32_t, uint32_t, uint32_t>> output;

  for (int i = 0; i < n; i++) {
    uint32_t start = nodes[i].beginning;
    uint32_t end = nodes[i].end;
    for (int j = start + 1; j < end; j++) {
      if (!is_null(edges.items[j].value)) {
        output.push_back(make_tuple(i, edges.items[j].dest, edges.items[j].value));
      }
    }
  }
  return output;
}

uint64_t PCSR::get_n() const { return nodes.size(); }

uint64_t PCSR::get_size() {
  uint64_t size = nodes.capacity() * sizeof(node_t);
  size += edges.N * sizeof(edge_t);
  return size;
}

void PCSR::print_array() {
  for (int i = 100000; i < 100500; i++) {
    if (is_null(edges.items[i].value)) {
      printf("%d-x ", i);
    } else if (is_sentinel(edges.items[i])) {
      uint32_t value = edges.items[i].value;
      if (value == UINT32_MAX) {
        value = 0;
      }
      printf("\n%d-s(%u):(%d, %d) ", i, value, nodes[value].beginning, nodes[value].end);
    } else {
      printf("%d-(%d, %u) ", i, edges.items[i].dest, edges.items[i].value);
    }
  }
  printf("\n\n");
}

// get density of a node
double get_density(edge_list_t *list, int index, int len) {
  int full = 0;
  for (int i = index; i < index + len; i++) {
    full += (!is_null(list->items[i].value));
  }
  double full_d = (double)full;
  return full_d / len;
}

double get_full(edge_list_t *list, int index, int len) {
  int full = 0;
  for (int i = 0; i < index + len; i++) {
    full += (!is_null(list->items[i].value));
  }
  return (double)full;
}

// height of this node in the tree
int get_depth(edge_list_t *list, int len) { return bsr_word(list->N / len); }

// get parent of this node in the tree
pair<int, int> get_parent(edge_list_t *list, int index, int len) {
  int parent_len = len * 2;
  int depth = get_depth(list, len);

  return make_pair(parent_len, depth);
}

// when adjusting the list size, make sure you're still in the
// density bound
pair_double density_bound(edge_list_t *list, int depth) {
  pair_double pair;

  // between 1/4 and 1/2
  //   pair.x = 1.0/2.0 - (( .25*depth)/list->H);
  // between 1/8 and 1/4
  pair.x = 1.0 / 4.0 - ((0.125 * depth) / list->H);
  pair.y = 3.0 / 4.0 + ((.25 * depth) / list->H);
  return pair;
}

// fix pointer from node to moved sentinel
void PCSR::fix_sentinel(int32_t node_index, int in) {
  nodes[node_index].beginning = in;
  if (node_index > 0) {
    nodes[node_index - 1].end = in;
  }
  if (node_index == nodes.size() - 1) {
    nodes[node_index].end = edges.N - 1;
  }
}

// Evenly redistribute elements in the ofm, given a range to look into
// index: starting position in ofm structure
// len: area to redistribute
void PCSR::redistribute(int index, int len) {
  edge_t *space = (edge_t *)malloc(len * sizeof(*(edges.items)));
  int j = 0;

  // move all items in ofm in the range into
  // a temp array
  for (int i = index; i < index + len; i++) {
    space[j] = edges.items[i];
    // counting non-null edges
    j += (!is_null(edges.items[i].value));
    // setting section to null
    edges.items[i].src = -1;
    edges.items[i].value = 0;
    edges.items[i].dest = 0;
  }

  // evenly redistribute for a uniform density
  double index_d = index;
  double step = ((double)len) / j;
  for (int i = 0; i < j; i++) {
    int in = static_cast<int>(index_d);

    edges.items[in] = space[i];
    if (is_sentinel(space[i])) {
      // fixing pointer of node that goes to this sentinel
      uint32_t node_index = space[i].value;
      if (node_index == UINT32_MAX) {
        node_index = 0;
      }
      fix_sentinel(node_index, in);
    }
    index_d += step;
  }
  free(space);
}

void PCSR::double_list() {
  int prev_locks_size = edges.N / edges.logN;
  edges.N *= 2;
  edges.logN = (1 << bsr_word(bsr_word(edges.N) + 1));
  edges.H = bsr_word(edges.N / edges.logN);
  // Added by Eleni Alevra - START
  if (is_numa_available) {
    edges.node_locks = (HybridLock **)numa_realloc(edges.node_locks, prev_locks_size * sizeof(HybridLock *),
                                                   (edges.N / edges.logN) * sizeof(HybridLock *));
    checkAllocation(edges.node_locks);
  } else {
    edges.node_locks = (HybridLock **)realloc(edges.node_locks, (edges.N / edges.logN) * sizeof(HybridLock *));
  }
  for (int i = prev_locks_size; i < edges.N / edges.logN; i++) {
    edges.node_locks[i] = new HybridLock();
  }
  // Added by Eleni Alevra - END

  if (is_numa_available) {
    edges.items =
        (edge_t *)numa_realloc(edges.items, (edges.N / 2) * sizeof(*(edges.items)), edges.N * sizeof(*(edges.items)));
  } else {
    edges.items = (edge_t *)realloc(edges.items, edges.N * sizeof(*(edges.items)));
  }

  for (int i = edges.N / 2; i < edges.N; i++) {
    edges.items[i].value = 0;  // setting second half to null
    edges.items[i].dest = 0;   // setting second half to null
  }

  redistribute(0, edges.N);
}

void PCSR::half_list() {
  int prev_locks_size = (edges.N / edges.logN);
  edges.N /= 2;
  edges.logN = (1 << bsr_word(bsr_word(edges.N) + 1));
  edges.H = bsr_word(edges.N / edges.logN);
  edge_t *new_array;
  if (is_numa_available) {
    new_array = (edge_t *)numa_alloc_onnode(edges.N * sizeof(*(edges.items)), domain);
    checkAllocation(new_array);
  } else {
    new_array = (edge_t *)malloc(edges.N * sizeof(*(edges.items)));
  }

  int j = 0;
  for (int i = 0; i < edges.N * 2; i++) {
    if (!is_null(edges.items[i].value)) {
      new_array[j++] = edges.items[i];
    }
  }
  // set remaining elements to null
  for (; j < edges.N; j++) {
    new_array[j].value = 0;
    new_array[j].dest = 0;
  }

  for (int i = (edges.N / edges.logN); i < prev_locks_size; i++) {
    edges.node_locks[i]->unlock();
    delete edges.node_locks[i];
  }

  if (is_numa_available) {
    edges.node_locks = (HybridLock **)numa_realloc(edges.node_locks, prev_locks_size * sizeof(HybridLock *),
                                                   (edges.N / edges.logN) * sizeof(HybridLock *));
    checkAllocation(edges.node_locks);
    numa_free(edges.items, edges.N * 2 * sizeof(*(edges.items)));
  } else {
    edges.node_locks = (HybridLock **)realloc(edges.node_locks, (edges.N / edges.logN) * sizeof(HybridLock *));
    free(edges.items);
  }

  edges.items = new_array;
  redistribute(0, edges.N);
}

// index is the beginning of the sequence that you want to slide right.
// notice that slide right does not not null the current spot.
// this is ok because we will be putting something in the current index
// after sliding everything to the right.
int PCSR::slide_right(int index, uint32_t src) {
  int rval = 0;
  edge_t el = edges.items[index];
  edges.items[index].src = -1;
  edges.items[index].dest = 0;
  edges.items[index].value = 0;
  index++;
  while (index < edges.N && !is_null(edges.items[index].value)) {
    edge_t temp = edges.items[index];
    edges.items[index] = el;
    if (!is_null(el.value) && is_sentinel(el)) {
      // fixing pointer of node that goes to this sentinel
      uint32_t node_index = el.value;
      if (node_index == UINT32_MAX) {
        node_index = 0;
      }
      fix_sentinel(node_index, index);
    }
    el = temp;
    index++;
  }
  if (!is_null(el.value) && is_sentinel(el)) {
    // fixing pointer of node that goes to this sentinel
    uint32_t node_index = el.value;
    if (node_index == UINT32_MAX) {
      node_index = 0;
    }
    fix_sentinel(node_index, index);
  }
  if (index == edges.N) {
    index--;
    slide_left(index, src);
    rval = -1;
    printf("slide off the end on the right, should be rare\n");
  }
  edges.items[index] = el;
  return rval;
}

// only called in slide right if it was going to go off the edge
// since it can't be full this doesn't need to worry about going off the other
// end
void PCSR::slide_left(int index, uint32_t src) {
  edge_t el = edges.items[index];
  edges.items[index].src = -1;
  edges.items[index].dest = 0;
  edges.items[index].value = 0;

  index--;
  while (index >= 0 && !is_null(edges.items[index].value)) {
    edge_t temp = edges.items[index];
    edges.items[index] = el;
    if (!is_null(el.value) && is_sentinel(el)) {
      // fixing pointer of node that goes to this sentinel
      uint32_t node_index = el.value;
      if (node_index == UINT32_MAX) {
        node_index = 0;
      }

      fix_sentinel(node_index, index);
    }
    el = temp;
    index--;
  }

  if (index == -1) {
    double_list();

    slide_right(0, src);
    index = 0;
  }
  if (!is_null(el.value) && is_sentinel(el)) {
    // fixing pointer of node that goes to this sentinel
    uint32_t node_index = el.value;
    if (node_index == UINT32_MAX) {
      node_index = 0;
    }
    fix_sentinel(node_index, index);
  }

  edges.items[index] = el;
}

// given index, return the starting index of the leaf it is in
int find_leaf(edge_list_t *list, int index) { return (index / list->logN) * list->logN; }

// true if e1, e2 are equals
bool edge_equals(edge_t e1, edge_t e2) { return e1.dest == e2.dest && e1.value == e2.value; }

// return index of the edge elem
// takes in edge list and place to start looking
uint32_t find_elem_pointer(edge_list_t *list, uint32_t index, edge_t elem) {
  edge_t item = list->items[index];
  while (!edge_equals(item, elem)) {
    item = list->items[++index];
  }
  return index;
}

// return index of the edge elem
// takes in edge list and place to start looking
// looks in reverse
uint32_t find_elem_pointer_reverse(edge_list_t *list, uint32_t index, edge_t elem) {
  edge_t item = list->items[index];
  while (!edge_equals(item, elem)) {
    item = list->items[--index];
  }
  return index;
}

// important: make sure start, end don't include sentinels
// returns the index of the smallest element bigger than you in the range
// [start, end) if no such element is found, returns end (because insert shifts
// everything to the right)
// also returns the version number of the node we will insert to
// this is to check if it has changed when we lock to do the insertion
// This function was modified for Eleni Alevra's implementation to return the version number and to do
// unlocking when unlock is set
pair<uint32_t, int> PCSR::binary_search(edge_t *elem, uint32_t start, uint32_t end, bool unlock) {
  int ins_v = -1;
  int start_node = find_leaf(&edges, start) / edges.logN;
  int end_node = find_leaf(&edges, end) / edges.logN;

  while (start + 1 < end) {
    // TODO: fix potential overflow for large data sets (use std::midpoint)
    const uint32_t mid = (start + end) / 2;
    //    elems++;
    edge_t item = edges.items[mid];
    uint32_t change = 1;
    uint32_t check = mid;

    bool flag = true;
    while (is_null(item.value) && flag) {
      flag = false;
      check = mid + change;
      if (check < end) {
        flag = true;
        if (check <= end) {
          //          elems++;
          item = edges.items[check];
          if (!is_null(item.value) || check == end) {
            break;
          }
        }
      }
      check = mid - change;
      if (check >= start) {
        flag = true;
        //        elems++;
        item = edges.items[check];
      }
      change++;
    }

    ins_v = edges.node_locks[find_leaf(&edges, check) / edges.logN]->load();
    int ins2 = edges.node_locks[find_leaf(&edges, mid) / edges.logN]->load();
    if (is_null(item.value) || start == check || end == check) {
      nodes_unlock_shared(unlock, start_node, end_node);
      if (!is_null(item.value) && start == check && elem->dest <= item.dest) {
        return make_pair(check, ins_v);
      } else {
        return make_pair(mid, ins2);
      }
    }

    // if we found it, return
    ins_v = edges.node_locks[find_leaf(&edges, check) / edges.logN]->load();
    if (elem->dest == item.dest) {
      nodes_unlock_shared(unlock, start_node, end_node);
      return make_pair(check, ins_v);
    } else if (elem->dest < item.dest) {
      end = check;  // if the searched for item is less than current item, set end
    } else {
      start = check;
      // otherwise, searched for item is more than current and we set start
    }
  }
  if (end < start) {
    start = end;
  }
  // handling the case where there is one element left
  // if you are leq, return start (index where elt is)
  // otherwise, return end (no element greater than you in the range)
  // printf("start = %d, end = %d, n = %d\n", start,end, list->N);
  ins_v = edges.node_locks[find_leaf(&edges, start) / edges.logN]->load();
  if (elem->dest <= edges.items[start].dest && !is_null(edges.items[start].value)) {
    nodes_unlock_shared(unlock, start_node, end_node);
    return make_pair(start, ins_v);
  }
  ins_v = edges.node_locks[find_leaf(&edges, end) / edges.logN]->load();
  nodes_unlock_shared(unlock, start_node, end_node);
  // Could also be null but it's end
  return make_pair(end, ins_v);
}

uint32_t PCSR::find_value(uint32_t src, uint32_t dest) {
  edge_t e;
  e.value = 0;
  e.dest = dest;
  pair<uint32_t, int> bs = binary_search(&e, nodes[src].beginning + 1, nodes[src].end, false);
  uint32_t loc = bs.first;
  if (!is_null(edges.items[loc].value) && edges.items[loc].dest == dest) {
    return edges.items[loc].value;
  } else {
    return 0;
  }
}

// insert elem at index returns index that the element went to (which
// may not be the same one that you put it at)
void PCSR::insert(uint32_t index, edge_t elem, uint32_t src, insertion_info_t *info) {
  int node_index = find_leaf(&edges, index);
  int level = edges.H;
  int len = edges.logN;

  // always deposit on the left
  if (!is_null(edges.items[index].value)) {
    // if the edge already exists in the graph, update its value
    // do not make another edge
    // return index of the edge that already exists
    if (!is_sentinel(elem) && edges.items[index].dest == elem.dest) {
      edges.items[index].value = elem.value;
      return;
    }
    if (index == edges.N - 1) {
      // when adding to the end double then add edge
      double_list();
      node_t node = nodes[src];
      // If we are at this point we already have a global lock on the data structure so there is no need to
      // do any extra locking for binary search
      uint32_t loc_to_add = binary_search(&elem, node.beginning + 1, node.end, false).first;
      return insert(loc_to_add, elem, src, nullptr);
    } else {
      if (slide_right(index, src) == -1) {
        index -= 1;
        slide_left(index, src);
      }
    }
  }
  edges.items[index].src = elem.src;
  edges.items[index].value = elem.value;
  edges.items[index].dest = elem.dest;

  double density = get_density(&edges, node_index, len);

  // spill over into next level up, node is completely full.
  if (density == 1) {
    node_index = find_node(node_index, len * 2);
    redistribute(node_index, len * 2);
  } else {
    redistribute(node_index, len);
  }

  // get density of the leaf you are in
  pair_double density_b = density_bound(&edges, level);
  density = get_density(&edges, node_index, len);

  // while density too high, go up the implicit tree
  // go up to the biggest node above the density bound
  if (info != nullptr) {
    // We have information about how much to redistribute from when we acquired locks
    if (info->double_list) {
      double_list();
      return;
    } else {
      len = info->max_len;
      node_index = info->node_index_final;
    }
  } else {
    while (density >= density_b.y) {
      len *= 2;
      if (len <= edges.N) {
        level--;
        node_index = find_node(node_index, len);
        density_b = density_bound(&edges, level);
        density = get_density(&edges, node_index, len);
      } else {
        // if you reach the root, double the list
        double_list();
        return;
      }
    }
  }
  if (len > edges.logN) {
    redistribute(node_index, len);
  }
}

void PCSR::remove(uint32_t index, const edge_t &elem, uint32_t src) {
  int node_index = find_leaf(&edges, index);
  // printf("node_index = %d\n", node_index);
  int level = edges.H;
  int len = edges.logN;

  if (is_null(edges.items[index].value)) {
    return;
  } else {
    if (is_sentinel(elem) || edges.items[index].dest != elem.dest) {
      return;
    }

    edges.items[index].value = 0;
    edges.items[index].dest = 0;
  }

  redistribute(node_index, len);
  // get density of the leaf you are in
  pair_double density_b = density_bound(&edges, level);
  double density = get_density(&edges, node_index, len);

  // while density too low, go up the implicit tree
  // go up to the biggest node above the density bound
  while (density < density_b.x) {
    len *= 2;
    if (len <= edges.N) {
      level--;
      node_index = find_node(node_index, len);
      density_b = density_bound(&edges, level);
      density = get_density(&edges, node_index, len);
    } else {
      // if you reach the root, halve the list
      half_list();
      return;
    }
  }
  redistribute(node_index, len);
}

// find index of edge
uint32_t find_index(edge_list_t *list, edge_t *elem_pointer) {
  edge_t *array_start = list->items;
  uint32_t index = (elem_pointer - array_start);
  return index;
}

std::vector<uint32_t> PCSR::sparse_matrix_vector_multiplication(std::vector<uint32_t> const &v) {
  std::vector<uint32_t> result(nodes.size(), 0);

  int num_vertices = nodes.size();

  for (int i = 0; i < num_vertices; i++) {
    // +1 to avoid sentinel

    for (uint32_t j = nodes[i].beginning + 1; j < nodes[i].end; j++) {
      result[i] += edges.items[j].value * v[edges.items[j].dest];
    }
  }
  return result;
}

// Prints neighbours of vertex src
void PCSR::print_graph(int src) {
  int num_vertices = nodes.size();
  for (int i = 0; i < num_vertices; i++) {
    // +1 to avoid sentinel
    //    int matrix_index = 0;
    if (i != src) continue;

    for (uint32_t j = nodes[i].beginning + 1; j < nodes[i].end; j++) {
      if (!is_null(edges.items[j].value)) {
        printf("%d ", edges.items[j].dest);
        //        while (matrix_index < edges.items[j].dest) {
        //          printf("000 ");
        //          matrix_index++;
        //        }
        //        printf("%03d ", edges.items[j].value);
        //        matrix_index++;
      }
    }
    //    for (uint32_t j = matrix_index; j < num_vertices; j++) {
    //      printf("000 ");
    //    }
    printf("\n");
  }
}

// add a node to the graph
void PCSR::add_node() {
  adding_sentinels = true;
  node_t node;
  int len = nodes.size();
  edge_t sentinel;
  sentinel.src = len;
  sentinel.dest = UINT32_MAX;  // placeholder
  sentinel.value = len;        // back pointer

  if (len > 0) {
    node.beginning = nodes[len - 1].end;
    node.end = node.beginning + 1;
  } else {
    node.beginning = 0;
    node.end = 1;
    sentinel.value = UINT32_MAX;
  }
  node.num_neighbors = 0;

  nodes.push_back(node);
  insert(node.beginning, sentinel, nodes.size() - 1, nullptr);
  adding_sentinels = false;
}

// This function was re-written for Eleni Alevra's implementation
void PCSR::add_edge(uint32_t src, uint32_t dest, uint32_t value) { add_edge_parallel(src, dest, value, 0); }

// Added by me
void PCSR::remove_edge(uint32_t src, uint32_t dest) {
  edge_t e;
  e.src = src;
  e.dest = dest;
  e.value = 1;

  edges.global_lock->lock_shared();

  int beginning = nodes[src].beginning;
  int end = nodes[src].end;
  uint32_t first_node = get_node_id(find_leaf(&edges, beginning + 1));
  uint32_t last_node = get_node_id(find_leaf(&edges, end));
  uint32_t loc_to_rem;
  int ins_node_v;
  if (lock_bsearch) {
    for (uint32_t i = first_node; i <= last_node; i++) {
      edges.node_locks[i]->lock_shared();
    }
    if (nodes[src].beginning != beginning || nodes[src].end != end) {
      release_locks_no_inc(make_pair(first_node, last_node));
      edges.global_lock->unlock_shared();
      remove_edge(src, dest);
      return;
    }
    loc_to_rem = binary_search(&e, beginning + 1, end, false).first;
    // Keep the version number of the PCSR node we will remove from so that if by the time we lock it has changed we
    // can re-start. We can't keep the PCSR node locked after binary search in case we have to acquire some locks to
    // its left first.
    ins_node_v = edges.node_locks[get_node_id(find_leaf(&edges, loc_to_rem))]->load();
    for (int i = first_node; i <= last_node; i++) {
      edges.node_locks[i]->unlock_shared();
    }
  } else {
    pair<int, int> bs = binary_search(&e, nodes[src].beginning + 1, nodes[src].end, false);
    loc_to_rem = bs.first;
    ins_node_v = bs.second;
  }

  nodes[src].num_neighbors--;

  pair<int, int> acquired_locks = acquire_remove_locks(loc_to_rem, e, src, ins_node_v, -1);
  if (acquired_locks.first == EDGE_NOT_FOUND) {
    cout << "not found " << src << " " << dest << endl;
    edges.global_lock->unlock_shared();
    return;
  }
  if (acquired_locks.first == NEED_GLOBAL_WRITE) {
    // we need to halve the array
    // release all node locks
    release_locks_no_inc({0, edges.N / edges.logN - 1});
    edges.global_lock->unlock_shared();
    const std::lock_guard<HybridLock> lck(*edges.global_lock);
    loc_to_rem = binary_search(&e, nodes[src].beginning + 1, nodes[src].end, false).first;
    remove(loc_to_rem, e, src);
  } else if (acquired_locks.first == NEED_RETRY) {
    // we need to re-start because when we acquired the locks things had changed
    nodes[src].num_neighbors++;
    edges.global_lock->unlock_shared();
    remove_edge(src, dest);
  } else {
    remove(loc_to_rem, e, src);
    release_locks(acquired_locks);
    edges.global_lock->unlock_shared();
  }
}

PCSR::PCSR(uint32_t init_n, uint32_t src_n, bool lock_search, int domain)
    : nodes(src_n), is_numa_available{numa_available() >= 0 && domain >= 0}, domain(domain) {
  edges.N = 2 << bsr_word(init_n + src_n);
  edges.logN = (1 << bsr_word(bsr_word(edges.N) + 1));
  edges.H = bsr_word(edges.N / edges.logN);
  edges.global_lock = make_shared<HybridLock>();

  lock_bsearch = lock_search;
  if (is_numa_available) {
    edges.node_locks = (HybridLock **)numa_alloc_onnode((edges.N / edges.logN) * sizeof(HybridLock *), domain);
    checkAllocation(edges.node_locks);

    edges.items = (edge_t *)numa_alloc_onnode(edges.N * sizeof(*(edges.items)), domain);
    checkAllocation(edges.items);
  } else {
    edges.node_locks = (HybridLock **)malloc((edges.N / edges.logN) * sizeof(HybridLock *));
    edges.items = (edge_t *)malloc(edges.N * sizeof(*(edges.items)));
  }

  for (int i = 0; i < edges.N / edges.logN; i++) {
    edges.node_locks[i] = new HybridLock();
  }

  double index_d = 0.0;
  const double step = ((double)edges.N) / src_n;
  int in = 0;

  for (int i = 0; i < src_n; i++) {
    if (i == 0) {
      nodes[i].beginning = 0;
    } else {
      nodes[i].beginning = nodes[i - 1].end;
    }
    index_d += step;
    in = static_cast<int>(index_d);
    nodes[i].end = in;
    nodes[i].num_neighbors = 0;
  }
  if (src_n != 0) {
    nodes[nodes.size() - 1].end = edges.N - 1;
  }

  index_d = 0.0;
  in = 0;
  int current = 0;

  // evenly distribute for a uniform density
  for (int i = 0; i < edges.N; i++) {
    if (i == in && current < src_n) {
      edges.items[i].src = current;
      edges.items[i].dest = UINT32_MAX;  // placeholder
      if (i == 0) {
        edges.items[i].value = UINT32_MAX;
      } else {
        edges.items[i].value = current;  // back pointer
      }
      current++;
      index_d += step;
      in = static_cast<int>(index_d);
    } else {
      edges.items[i].src = -1;
      edges.items[i].dest = 0;
      edges.items[i].value = 0;
    }
  }
}

PCSR::~PCSR() {
  if (is_numa_available) {
    numa_free(edges.items, edges.N * sizeof(*(edges.items)));
    for (int i = 0; i < (edges.N / edges.logN); i++) {
      delete edges.node_locks[i];
    }
    numa_free(edges.node_locks, (edges.N / edges.logN) * sizeof(HybridLock *));
  } else {
    free(edges.items);
    for (int i = 0; i < (edges.N / edges.logN); i++) {
      delete edges.node_locks[i];
    }
    free(edges.node_locks);
  }
}

/**
 * The following functions were all added for Eleni Alevra's implementation.
 */

// Used for debugging
// Returns true if edge {src, dest} exists
// Added by Eleni Alevra
bool PCSR::edge_exists(uint32_t src, uint32_t dest) {
  node_t node = nodes[src];

  edge_t e;
  e.dest = dest;
  e.value = 1;
  uint32_t loc_to_rem = binary_search(&e, node.beginning + 1, node.end, false).first;
  e = edges.items[loc_to_rem];
  return !(is_null(e.value)) && !is_sentinel(e) && e.dest == dest;
}

// Used for debugging
// Returns true if every neighbourhood is sorted
// Added by Eleni Alevra
bool PCSR::is_sorted() const {
  for (int i = 0; i < nodes.size(); i++) {
    int prev = 0;
    for (int j = nodes[i].beginning + 1; j < nodes[i].end; j++) {
      if (!is_null(edges.items[j].value)) {
        if (edges.items[j].dest < prev) {
          cout << prev << " " << i << " " << edges.items[j].dest << endl;
          return false;
        }
        prev = edges.items[j].dest;
      }
    }
  }
  return true;
}

// Reads the neighbourhood of vertex src
// Added by Eleni Alevra
void PCSR::read_neighbourhood(int src) {
  if (src < get_n()) {
    int k = 0;
    for (int i = nodes[src].beginning + 1; i < nodes[src].end; i++) {
      k = edges.items[i].dest;
    }
  }
}

vector<int> PCSR::get_neighbourhood(int src) const {
  std::vector<int> neighbours;
  if (src < get_n()) {
    neighbours.reserve(nodes[src].num_neighbors);
    for (int i = nodes[src].beginning + 1; i < nodes[src].end; i++) {
      if (edges.items[i].value != 0) {
        neighbours.push_back(edges.items[i].dest);
      }
    }
  }
  return neighbours;
}

// Get id of PCSR node (starting from 0)
// e.g. if every PCSR node has 8 elements, index number 5 is in PCSR node 0, index number 8 is in PCSR node 1 etc.
// Added by Eleni Alevra
uint32_t PCSR::get_node_id(uint32_t node_index) { return node_index / edges.logN; }

// Release acquired locks and increment the version counters to notify any other thread that will acquire them
// that a change has happened
// Added by Eleni Alevra
void PCSR::release_locks(pair<int, int> acquired_locks) {
  for (int i = acquired_locks.first; i <= acquired_locks.second; i++) {
    ++(*edges.node_locks[i]);
    edges.node_locks[i]->unlock();
  }
}

// Release acquired locks without incrementing version counters (we didn't make any changes to these PCSR nodes)
// Added by Eleni Alevra
void PCSR::release_locks_no_inc(pair<int, int> acquired_locks) {
  for (int i = acquired_locks.first; i <= acquired_locks.second; i++) {
    edges.node_locks[i]->unlock();
  }
}

// Acquire locks required to insert an edge
// Returns id of first and last node locked and a struct with information about redistribute to avoid repeating checks
// index: where the new edge should be inserted
// elem: the edge to insert
// src: source vertex
// ins_node_v: the version number of the PCSR node we want to insert to, at the time when binary search happened.
// We use this to verify nothing has changed when we lock it.
// left_node_bound: the leftmost PCSR node to lock from, initially this will be the node where we want to insert but
// during redistribute we might have to lock some extra PCSR nodes to the left so to avoid deadlocks we release the
// locks we already have and re-start acquiring from the new leftmost PCSR node
// tries: how many times we have re-tried locking, to make sure we don't re-try too many times
// Added by Eleni Alevra
pair<pair<int, int>, insertion_info_t *> PCSR::acquire_insert_locks(uint32_t index, edge_t elem, uint32_t src,
                                                                    int ins_node_v, uint32_t left_node_bound,
                                                                    int tries) {
  if (tries > 3) {
    // very rarely happens (about 100 times in 14M insertions)
    return make_pair(make_pair(NEED_GLOBAL_WRITE, NEED_GLOBAL_WRITE), nullptr);
  }
  int node_index = find_leaf(&edges, index);
  int init_node_index = node_index;
  int level = edges.H;
  int len = edges.logN;
  uint32_t min_node = get_node_id(node_index);
  uint32_t max_node = min_node;
  uint32_t node_id = get_node_id(node_index);
  if (left_node_bound != -1) {
    uint32_t leftmost_node = left_node_bound;
    for (int i = leftmost_node; i <= node_id; i++) {
      edges.node_locks[i]->lock();
    }
    //    if (node_id < (edges.N / edges.logN) - 1) {
    //      edges.node_locks[node_id + 1]->lock();
    //      max_node = node_id + 1;
    //    }
    //    edges.node_locks[node_id + 1]->lock();
    //    max_node = node_id + 1;
    min_node = min(min_node, leftmost_node);
  } else {
    if (node_id > 0 && !lock_bsearch) {
      edges.node_locks[node_id - 1]->lock();
      min_node = node_id - 1;
    }
    edges.node_locks[node_id]->lock();
    //    if (node_id < (edges.N / edges.logN) - 1) {
    //      edges.node_locks[node_id + 1]->lock();
    //      max_node = node_id + 1;
    //    }
  }
  if (ins_node_v != edges.node_locks[node_id]->load()) {
    for (int i = min_node; i <= max_node; i++) {
      edges.node_locks[i]->unlock();
    }
    return make_pair(make_pair(NEED_RETRY, NEED_RETRY), nullptr);
  }
  if (index == edges.N - 1 && !(is_null(edges.items[index].value))) {
    for (int i = min_node; i <= max_node; i++) {
      edges.node_locks[i]->unlock();
    }
    return make_pair(make_pair(NEED_GLOBAL_WRITE, NEED_GLOBAL_WRITE), nullptr);
  }
  if (!lock_bsearch) {
    // We didn't lock during binary search so we might have gotten back a wrong index, need to check and if it's wrong
    // re-try
    auto ins_edge = edges.items[index];
    if (!got_correct_insertion_index(ins_edge, src, index, elem, node_index, node_id, max_node, min_node)) {
      for (int i = min_node; i <= max_node; i++) {
        edges.node_locks[i]->unlock();
      }
      return make_pair(make_pair(NEED_RETRY, NEED_RETRY), nullptr);
    }
  }

  // check which locks we still need to acquire for redistribute

  if (get_density(&edges, node_index, len) + (1.0 / len) == 1) {
    uint32_t new_node_idx = find_node(node_index, 2 * len);
    uint32_t new_node_id = get_node_id(new_node_idx);
    if (new_node_idx == node_index && new_node_id > max_node) {
      edges.node_locks[new_node_id]->lock();
      max_node = new_node_id;
    } else if (new_node_id < min_node) {
      release_locks_no_inc(make_pair(min_node, max_node));
      return acquire_insert_locks(index, elem, src, ins_node_v, new_node_id, tries + 1);
    }
    node_index = new_node_idx;
  }

  pair_double density_b = density_bound(&edges, level);
  double density = get_density(&edges, node_index, len) + (1.0 / len);

  while (density >= density_b.y) {
    len *= 2;
    if (len <= edges.N) {
      level--;
      uint32_t new_node_index = find_node(node_index, len);
      if (new_node_index < node_index) {
        uint32_t new_node_id = get_node_id(new_node_index);
        if (new_node_id < min_node) {
          release_locks_no_inc(make_pair(min_node, max_node));
          return acquire_insert_locks(index, elem, src, ins_node_v, get_node_id(new_node_index), tries + 1);
        }
        min_node = min(min_node, new_node_id);
        node_index = new_node_index;
      } else {
        uint32_t end = get_node_id(find_leaf(&edges, new_node_index + len));
        node_index = new_node_index;
        for (uint32_t i = max_node + 1; i < end; i++) {
          max_node = max(max_node, i);
          edges.node_locks[i]->lock();
          //          got_locks++;
        }
      }
      density_b = density_bound(&edges, level);
      density = get_density(&edges, node_index, len) + (1.0 / len);
    } else {
      for (int i = min_node; i <= max_node; i++) {
        edges.node_locks[i]->unlock();
      }
      insertion_info_t *info = (insertion_info_t *)malloc(sizeof(insertion_info_t));

      info->double_list = true;
      return make_pair(make_pair(NEED_GLOBAL_WRITE, NEED_GLOBAL_WRITE), info);
    }
  }
  uint32_t new_node_index = find_node(node_index, len);
  if (new_node_index < node_index) {
    uint32_t node_id = get_node_id(new_node_index);
    if (node_id < min_node) {
      release_locks_no_inc(make_pair(min_node, max_node));
      return acquire_insert_locks(index, elem, src, ins_node_v, get_node_id(new_node_index), tries + 1);
    }
    min_node = min(min_node, get_node_id(new_node_index));
  } else {
    uint32_t end = get_node_id(find_leaf(&edges, new_node_index + len));
    for (uint32_t i = max_node + 1; i < end; i++) {
      max_node = max(max_node, i);
      //      got_locks++;
      edges.node_locks[i]->lock();
    }
  }
  node_index = new_node_index;

  // lock PCSR nodes needed for slide_right / slide_left
  insertion_info_t *info = (insertion_info_t *)malloc(sizeof(insertion_info_t));
  info->double_list = false;
  info->max_len = len;
  info->node_index_final = node_index;
  len = edges.logN;
  node_index = find_leaf(&edges, index);

  if (!(is_null(edges.items[index].value))) {
    uint32_t curr_node = get_node_id(node_index);
    uint32_t curr_ind = index + 1;
    uint32_t curr_node_idx = node_index;
    if (curr_ind < edges.N && curr_ind >= curr_node_idx + len) {
      curr_node_idx = curr_ind;
      curr_node++;
      if (curr_node > max_node) {
        edges.node_locks[curr_node]->lock();
        max_node = curr_node;
      }
    }
    while (curr_ind < edges.N && !(is_null(edges.items[curr_ind].value))) {
      if (++curr_ind < edges.N && curr_ind >= curr_node_idx + len) {
        curr_node++;
        if (curr_node > max_node) {
          edges.node_locks[curr_node]->lock();
          max_node = curr_node;
        }
        curr_node_idx = curr_ind;
      }
    }
    if (curr_ind == edges.N) {
      curr_ind = index;
      curr_node = get_node_id(node_index);
      curr_node_idx = node_index;
      while (curr_ind >= 0 && !(is_null(edges.items[curr_ind].value))) {
        if (--curr_ind >= 0 && curr_ind < curr_node_idx) {
          curr_node_idx = find_leaf(&edges, curr_ind);
          curr_node--;
          if (curr_node < min_node) {
            min_node = curr_node;
            release_locks_no_inc(make_pair(min_node, max_node));
            return acquire_insert_locks(index, elem, src, ins_node_v, curr_node, tries + 1);
          }
        }
      }
      if (curr_ind == -1) {
        for (int i = min_node; i <= max_node; i++) {
          edges.node_locks[i]->unlock();
        }
        return make_pair(make_pair(NEED_GLOBAL_WRITE, NEED_GLOBAL_WRITE), nullptr);
      }
    }
  }
  return make_pair(make_pair(min_node, max_node), info);
}

// Acquire locks required to remove an edge
// Returns id of first and last node locked
// index: where the edge to remove is
// elem: the edge to remove
// src: source vertex
// ins_node_v: the version number of the PCSR node we want to remove from, at the time when binary search happened
// we use this to verify nothing has changed when we lock it
// left_node_bound: the leftmost PCSR node to lock from, initially this will be the node where the edge is but
// during redistribute we might have to lock some extra PCSR nodes to the left so to avoid deadlocks we release the
// locks we already have and re-start acquiring from the new leftmost PCSR node
// Added by Eleni Alevra
pair<int, int> PCSR::acquire_remove_locks(uint32_t index, edge_t elem, uint32_t src, int ins_node_v,
                                          uint32_t left_node_bound) {
  int node_index = find_leaf(&edges, index);
  // printf("node_index = %d\n", node_index);
  int level = edges.H;
  int len = edges.logN;
  int node_id = get_node_id(node_index);
  uint32_t min_node = node_id;
  uint32_t max_node = node_id;

  // If we have a leftmost PCSR start locking from it
  if (left_node_bound != -1) {
    for (uint32_t i = left_node_bound; i <= node_id; i++) {
      edges.node_locks[i]->lock();
      //      got_locks++;
    }
    min_node = left_node_bound;
  } else {
    edges.node_locks[node_id]->lock();
    //    got_locks++;
  }
  if (!got_correct_insertion_index(edges.items[index], src, index, elem, node_index, node_id, max_node, min_node)) {
    release_locks_no_inc(make_pair(min_node, max_node));
    //    retries++;
    return make_pair(NEED_RETRY, NEED_RETRY);
  }
  // We now have the lock for the PCSR node the edge is but things might have moved since binary search so we compare
  // its version number to the one during binary search to see if any changes have happened. If they have we re-start.
  if (edges.node_locks[node_id]->load() != ins_node_v) {
    release_locks_no_inc(make_pair(min_node, max_node));
    return make_pair(NEED_RETRY, NEED_RETRY);
  }
  if (is_null(edges.items[index].value)) {
    // Edge not found
    release_locks_no_inc(make_pair(min_node, max_node));
    return make_pair(EDGE_NOT_FOUND, EDGE_NOT_FOUND);
  } else {
    if (is_sentinel(elem) || edges.items[index].dest != elem.dest) {
      // Edge not found
      release_locks_no_inc(make_pair(min_node, max_node));
      return make_pair(EDGE_NOT_FOUND, EDGE_NOT_FOUND);
    }
  }

  // get density of the leaf you are in
  pair_double density_b = density_bound(&edges, level);
  double density = get_density(&edges, node_index, len) - (1.0 / len);

  // while density too low, go up the implicit tree
  // go up to the biggest node below the density bound
  while (density < density_b.x) {
    len *= 2;
    if (len <= edges.N) {
      level--;
      uint32_t new_node_idx = find_node(node_index, len);
      int new_node_id = get_node_id(new_node_idx);
      if (new_node_idx < node_index && new_node_id < min_node) {
        release_locks_no_inc(make_pair(min_node, max_node));
        return acquire_remove_locks(index, elem, src, ins_node_v, new_node_id);
      }
      for (uint32_t i = max_node + 1; i < get_node_id(new_node_idx + len); i++) {
        edges.node_locks[i]->lock();
        //        got_locks++;
        max_node = i;
      }
      node_index = new_node_idx;
      density_b = density_bound(&edges, level);
      density = get_density(&edges, node_index, len) - (1.0 / len);
    } else {
      return make_pair(NEED_GLOBAL_WRITE, NEED_GLOBAL_WRITE);
    }
  }

  uint32_t new_node_idx = find_node(node_index, len);
  int new_node_id = get_node_id(new_node_idx);
  if (new_node_idx < node_index && new_node_id < min_node) {
    release_locks_no_inc(make_pair(min_node, max_node));
    return acquire_remove_locks(index, elem, src, ins_node_v, new_node_id);
  }
  for (uint32_t i = max_node + 1; i < get_node_id(new_node_idx + len); i++) {
    edges.node_locks[i]->lock();
    //    got_locks++;
    max_node = i;
  }
  return make_pair(min_node, max_node);
}

// Returns total number of edges in the array
// Added by Eleni Alevra
int PCSR::count_total_edges() {
  int t = 0;
  for (int i = 0; i < nodes.size(); i++) {
    for (int j = nodes[i].beginning + 1; j < nodes[i].end; j++) {
      if (!(is_null(edges.items[j].value))) {
        t++;
      }
    }
  }
  return t;
}

// Used for parallel re-distributing
// Stores the elements in the range [index, index + len) in array space and returns the redistribution step
// and the number of elements
// Added by Eleni Alevra
pair<double, int> PCSR::redistr_store(edge_t *space, int index, int len) {
  int j = 0;
  for (int i = index; i < index + len; i++) {
    space[j] = edges.items[i];
    j += (!(is_null(edges.items[i].value)));
    edges.items[i].value = 0;
    edges.items[i].dest = 0;
  }
  return make_pair(((double)len) / j, j);
}

// Added by Eleni Alevra
PCSR::PCSR(uint32_t init_n, vector<condition_variable *> *cvs, bool lock_search, int domain)
    : is_numa_available{numa_available() >= 0 && domain >= 0}, domain(domain) {
  edges.N = 2 << bsr_word(init_n);
  edges.logN = (1 << bsr_word(bsr_word(edges.N) + 1));
  edges.H = bsr_word(edges.N / edges.logN);
  edges.global_lock = make_shared<HybridLock>();

  this->redistr_mutex = new mutex;
  this->redistr_cv = new condition_variable;
  this->redistr_cvs = cvs;
  lock_bsearch = lock_search;

  if (is_numa_available) {
    edges.node_locks = (HybridLock **)numa_alloc_onnode((edges.N / edges.logN) * sizeof(HybridLock *), domain);
    checkAllocation(edges.node_locks);
    edges.items = (edge_t *)numa_alloc_onnode(edges.N * sizeof(*(edges.items)), domain);
    checkAllocation(edges.items);
  } else {
    edges.node_locks = (HybridLock **)malloc((edges.N / edges.logN) * sizeof(HybridLock *));
    edges.items = (edge_t *)malloc(edges.N * sizeof(*(edges.items)));
  }
  for (int i = 0; i < edges.N; i++) {
    edges.items[i].src = -1;
    edges.items[i].value = 0;
    edges.items[i].dest = 0;
  }

  for (int i = 0; i < edges.N / edges.logN; i++) {
    edges.node_locks[i] = new HybridLock();
  }

  for (int i = 0; i < init_n; i++) {
    add_node();
  }
}

// Returns total number of edges in range [index, index + len)
// Added by Eleni Alevra
int PCSR::count_elems(int index, int len) {
  int j = 0;
  for (int i = index; i < index + len; i++) {
    j += !(is_null(edges.items[i].value)) && !is_sentinel(edges.items[i]);
  }
  return j;
}

// Returns true if the given edge should be inserted in index
// Added by Eleni Alevra
bool PCSR::got_correct_insertion_index(edge_t ins_edge, uint32_t src, uint32_t index, edge_t elem, int node_index,
                                       int node_id, uint32_t &max_node, uint32_t &min_node) {
  // Check that we are in the right neighbourhood
  if (!(is_null(ins_edge.value)) &&
      ((is_sentinel(ins_edge) && src != nodes.size() - 1 && ins_edge.src != src + 1) ||
       (is_sentinel(ins_edge) && src == nodes.size() - 1 && ins_edge.value != UINT32_MAX) ||
       (!is_sentinel(ins_edge) && ins_edge.src != src))) {
    return false;
  }
  // Check that the current edge is larger than the one we want to insert
  if (!(is_null(ins_edge.value)) && !is_sentinel(ins_edge) && ins_edge.dest < elem.dest) {
    return false;
  }
  if (is_null(ins_edge.value)) {
    // The current position is empty so we need to find the next element to the right to make sure it's bigger than the
    // one we want to insert
    int ind = index + 1;
    int curr_n = node_index;
    if (ind < edges.N && ind >= curr_n + edges.logN) {
      curr_n += edges.logN;
      edges.node_locks[++max_node]->lock();
    }
    while (ind < edges.N && is_null(edges.items[ind].value)) {
      ind++;
      if (ind < edges.N && ind >= curr_n + edges.logN) {
        curr_n += edges.logN;
        edges.node_locks[++max_node]->lock();
      }
    }

    if (ind < edges.N) {
      edge_t item = edges.items[ind];
      // if it's in the same neighbourhood and smaller we're in the wrong position
      if (!is_null(item.value) && !is_sentinel(item) && item.src == src && item.dest < elem.dest) {
        return false;
      }
      // if it's a sentinel node for the wrong vertex the index is wrong
      if (!(is_null(item.value)) && is_sentinel(item) &&
          ((src != nodes.size() - 1 && item.value != src + 1) ||
           (src == nodes.size() - 1 && item.value == UINT32_MAX))) {
        return false;
      }
    }
  }
  // Go to the left to find the next element to the left and make sure it's less than the one we are inserting
  int ind = index - 1;
  edge_t item;
  item.value = 0;
  item.dest = 0;
  item.src = -1;
  while (ind >= 0 && is_null(edges.items[ind].value)) {
    ind--;
  }
  item = edges.items[ind];
  if (!is_null(item.value) && !is_sentinel(item) && item.src == src && item.dest >= elem.dest) {
    return false;
  }
  if (!is_null(item.value) && is_sentinel(item) &&
      ((src == 0 && item.value != UINT32_MAX) || (src != 0 && item.value != src))) {
    return false;
  }
  return true;
}

void PCSR::add_edge_parallel(uint32_t src, uint32_t dest, uint32_t value, int retries) {
  if (value != 0 && src < get_n()) {
    edge_t e;
    e.src = src;
    e.dest = dest;
    e.value = value;
    if (retries > 3) {
      const std::lock_guard<HybridLock> lck(*edges.global_lock);
      nodes[src].num_neighbors++;
      int pos = binary_search(&e, nodes[src].beginning + 1, nodes[src].end, false).first;
      insert(pos, e, src, nullptr);
      return;
    }

    edges.global_lock->lock_shared();
    int beginning = nodes[src].beginning;
    int end = nodes[src].end;
    uint32_t first_node = get_node_id(find_leaf(&edges, beginning + 1));
    nodes[src].num_neighbors++;
    pair<uint32_t, int> bs;
    uint32_t loc_to_add;
    if (lock_bsearch) {
      uint32_t last_node = get_node_id(find_leaf(&edges, end));
      // Lock for binary search
      for (uint32_t i = first_node; i <= last_node; i++) {
        edges.node_locks[i]->lock_shared();
      }
      // If after we have locked there have been more edges added, re-start to include them in the search
      if (nodes[src].beginning != beginning || nodes[src].end != end) {
        for (int i = first_node; i <= last_node; i++) {
          edges.node_locks[i]->unlock_shared();
        }
        edges.global_lock->unlock_shared();
        nodes[src].num_neighbors--;
        add_edge_parallel(src, dest, value, retries + 1);
        return;
      }
      bs = binary_search(&e, beginning + 1, end, true);
      loc_to_add = bs.first;
    } else {
      // get back index where the new edge should go and the version number of its PCSR node when we read its value
      bs = binary_search(&e, beginning + 1, end, false);
      loc_to_add = bs.first;
      uint32_t index_node = get_node_id(find_leaf(&edges, loc_to_add));
      if (index_node < first_node) {
        nodes[src].num_neighbors--;
        edges.global_lock->unlock_shared();
        add_edge_parallel(src, dest, value, retries + 1);
        return;
      }
    }
    pair<pair<int, int>, insertion_info_t *> acquired_locks =
        acquire_insert_locks(loc_to_add, e, src, bs.second, -1, 0);
    if (acquired_locks.first.first == NEED_RETRY) {
      nodes[src].num_neighbors--;
      edges.global_lock->unlock_shared();
      add_edge_parallel(src, dest, value, retries + 1);
      return;
    }
    if (acquired_locks.first.first == NEED_GLOBAL_WRITE) {
      edges.global_lock->unlock_shared();
      const std::lock_guard<HybridLock> lck(*edges.global_lock);
      loc_to_add = binary_search(&e, nodes[src].beginning + 1, nodes[src].end, false).first;
      insert(loc_to_add, e, src, acquired_locks.second);
    } else {
      insert(loc_to_add, e, src, acquired_locks.second);
      release_locks(acquired_locks.first);
      edges.global_lock->unlock_shared();
    }
    free(acquired_locks.second);
  }
}

void PCSR::insert_nodes_and_edges_front(std::vector<node_t> new_nodes, std::vector<edge_t> new_edges) {}

void PCSR::insert_nodes_and_edges_back(std::vector<node_t> new_nodes, std::vector<edge_t> new_edges) {}

std::pair<std::vector<node_t>, std::vector<edge_t>> PCSR::remove_nodes_and_edges_front(int num_nodes) {
  std::vector<node_t> exported_nodes;
  std::vector<edge_t> exported_edges;
  return make_pair(exported_nodes, exported_edges);
}
std::pair<std::vector<node_t>, std::vector<edge_t>> PCSR::remove_nodes_and_edges_back(int num_nodes) {
  std::vector<node_t> exported_nodes;
  std::vector<edge_t> exported_edges;
  return make_pair(exported_nodes, exported_edges);
}