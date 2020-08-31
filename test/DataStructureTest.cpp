/**
 * @file DataStructureTest.cpp
 * @author Christian Menges
 */
#include "DataStructureTest.h"
#include "PPPCSR.h"

TEST_F(DataStructureTest, Initialization) {
  const int size = 10;
  PPPCSR pcsr(size, size, false, 1, false);
  EXPECT_EQ(pcsr.get_n(), size);
}

TEST_F(DataStructureTest, add_node) {
  PPPCSR pcsr(0, 0, false, 1, false);
  EXPECT_EQ(pcsr.get_n(), 0);
  pcsr.add_node();
  EXPECT_EQ(pcsr.get_n(), 1);
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 0);
}

TEST_F(DataStructureTest, add_edge) {
  PPPCSR pcsr(10, 10, false, 1, false);
  // Try to add edge without corresponding node
  pcsr.add_edge(11, 1, 1);

  pcsr.add_edge(0, 1, 1);
  EXPECT_TRUE(pcsr.edge_exists(0, 1));
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 1);
  EXPECT_EQ(pcsr.get_n(), 10);
  EXPECT_EQ(pcsr.get_neighbourhood(2).size(), 0);
}

TEST_F(DataStructureTest, remove_edge) {
  PPPCSR pcsr(10, 10, false, 1, false);
  pcsr.add_node();
  pcsr.remove_edge(0, 1);
  EXPECT_FALSE(pcsr.edge_exists(0, 1));
  pcsr.add_edge(0, 1, 1);
  EXPECT_TRUE(pcsr.edge_exists(0, 1));
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 1);
  pcsr.remove_edge(0, 1);
  EXPECT_FALSE(pcsr.edge_exists(0, 1));
  EXPECT_EQ(pcsr.get_neighbourhood(2).size(), 0);
}

TEST_F(DataStructureTest, add_remove_edge_1E4) {
  PCSR pcsr(10, 10, false, 0);
  constexpr int edge_count = 1E4;
  for (int i = 1; i < edge_count + 1; ++i) {
    pcsr.add_edge(0, i, i);
    EXPECT_TRUE(pcsr.edge_exists(0, i)) << i;
    // Check whether all locks were released
    for (int j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
      EXPECT_TRUE(pcsr.edges.node_locks[j]->lockable())
          << "Current iteration: " << i << " lock id: " << j;
    }
    EXPECT_TRUE(pcsr.edges.global_lock->lockable());
  }
  EXPECT_EQ(pcsr.get_n(), 10);
  EXPECT_EQ(pcsr.getNode(0).num_neighbors, edge_count);

  for (int i = 1; i < edge_count + 1; ++i) {
    pcsr.remove_edge(0, i);
    EXPECT_FALSE(pcsr.edge_exists(0, i)) << i;
    // Check whether all locks were released
    for (int j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
      EXPECT_TRUE(pcsr.edges.node_locks[j]->lockable())
          << "Current iteration: " << i << " lock id: " << j;
    }
    EXPECT_TRUE(pcsr.edges.global_lock->lockable());
  }
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 0);
  EXPECT_EQ(pcsr.get_n(), 10);
}