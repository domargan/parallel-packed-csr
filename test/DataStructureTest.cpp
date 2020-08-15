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