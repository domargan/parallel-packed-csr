/**
 * @file DataStructureTest.cpp
 * @author Christian Menges
 */
#include "DataStructureTest.h"
#include "PPPCSR.h"

TEST_F(DataStructureTest, Initialization) {
  const int size = 10;
  PPPCSR pcsr(size, size, false, 1);
  EXPECT_EQ(pcsr.get_n(), size);
}

TEST_F(DataStructureTest, add_node) {
  PPPCSR pcsr(0, 0, false, 1);
  EXPECT_EQ(pcsr.get_n(), 0);
  pcsr.add_node();
  EXPECT_EQ(pcsr.get_n(), 1);
}

TEST_F(DataStructureTest, add_edge) {
  PPPCSR pcsr(0, 0, false, 1);
  // Try to add edge without corresponding node
  pcsr.add_edge(0, 1, 1);
  pcsr.add_node();
  pcsr.add_edge(0, 1, 1);
  EXPECT_TRUE(pcsr.edge_exists(0, 1));
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 1);
  EXPECT_EQ(pcsr.get_n(), 1);
  EXPECT_EQ(pcsr.get_neighbourhood(2).size(), 0);
}

TEST_F(DataStructureTest, remove_edge) {
  PPPCSR pcsr(0, 0, false, 1);
  pcsr.add_node();
  pcsr.remove_edge(0, 1);
  EXPECT_FALSE(pcsr.edge_exists(0, 1));
  pcsr.add_edge(0, 1, 1);
  EXPECT_TRUE(pcsr.edge_exists(0, 1));
  pcsr.remove_edge(0, 1);
  EXPECT_FALSE(pcsr.edge_exists(0, 1));
}