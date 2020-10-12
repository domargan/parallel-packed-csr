/**
 * @file DataStructureTest.cpp
 * @author Christian Menges
 */
#include "DataStructureTest.h"
#include "PPPCSR.h"
#include "bfs.h"
#include "pagerank.h"

using ::testing::Bool;

TEST_P(DataStructureTest, Initialization) {
  const int size = 10;
  PPPCSR pcsr(size, size, GetParam(), 1, 1, false);
  EXPECT_EQ(pcsr.get_n(), size);
}

TEST_P(DataStructureTest, add_node) {
  PPPCSR pcsr(0, 0, GetParam(), 1, 1, false);
  EXPECT_EQ(pcsr.get_n(), 0);
  pcsr.add_node();
  EXPECT_EQ(pcsr.get_n(), 1);
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 0);
}

TEST_P(DataStructureTest, add_edge) {
  PPPCSR pcsr(10, 10, GetParam(), 1, 1, false);
  // Try to add edge without corresponding node
  pcsr.add_edge(11, 1, 1);

  pcsr.add_edge(0, 1, 1);
  EXPECT_TRUE(pcsr.edge_exists(0, 1));
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 1);
  EXPECT_EQ(pcsr.get_n(), 10);
  EXPECT_EQ(pcsr.get_neighbourhood(2).size(), 0);
}

TEST_P(DataStructureTest, remove_edge) {
  PPPCSR pcsr(10, 10, GetParam(), 1, 1, false);
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

TEST_P(DataStructureTest, add_remove_edge_1E4_seq) {
  PCSR pcsr(10, 10, GetParam(), 0);
  constexpr int edge_count = 1E4;
  for (int i = 1; i < edge_count + 1; ++i) {
    pcsr.add_edge(0, i, i);
    EXPECT_TRUE(pcsr.edge_exists(0, i)) << i;
    // Check whether all locks were released
    for (uint32_t j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
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
    for (uint32_t j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
      EXPECT_TRUE(pcsr.edges.node_locks[j]->lockable())
          << "Current iteration: " << i << " lock id: " << j;
    }
    EXPECT_TRUE(pcsr.edges.global_lock->lockable());
  }
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 0);
  EXPECT_EQ(pcsr.get_n(), 10);
}

TEST_P(DataStructureTest, add_remove_edge_1E5_par) {
  PCSR pcsr(10, 10, GetParam(), 0);
  constexpr int edge_count = 1E5;
#pragma omp parallel
  {
    pcsr.edges.global_lock->registerThread();
#pragma omp for nowait
    for (int i = 1; i < edge_count + 1; ++i) {
      pcsr.add_edge(0, i, i);
      EXPECT_TRUE(pcsr.edge_exists(0, i)) << i;
    }
    pcsr.edges.global_lock->unregisterThread();
  }

  // Check whether all locks were released
  for (uint32_t j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
    EXPECT_TRUE(pcsr.edges.node_locks[j]->lockable()) << "Lock id: " << j;
  }
  EXPECT_TRUE(pcsr.edges.global_lock->lockable());
  EXPECT_EQ(pcsr.get_n(), 10);
  EXPECT_EQ(pcsr.getNode(0).num_neighbors, edge_count);

#pragma omp parallel
  {
    pcsr.edges.global_lock->registerThread();
#pragma omp for nowait
    for (int i = 1; i < edge_count + 1; ++i) {
      pcsr.remove_edge(0, i);
      EXPECT_FALSE(pcsr.edge_exists(0, i)) << i;
    }
    pcsr.edges.global_lock->unregisterThread();
  }
  // Check whether all locks were released
  for (uint32_t j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
    EXPECT_TRUE(pcsr.edges.node_locks[j]->lockable()) << "Lock id: " << j;
  }
  EXPECT_TRUE(pcsr.edges.global_lock->lockable());
  EXPECT_EQ(pcsr.get_neighbourhood(0).size(), 0);
  EXPECT_EQ(pcsr.get_n(), 10);
}

TEST_P(DataStructureTest, add_remove_edge_random_2E4_seq) {
  PCSR pcsr(1000, 1000, GetParam(), 0);
  constexpr int edge_count = 2E4;
  for (int i = 1; i < edge_count + 1; ++i) {
    int src = std::rand() % 1000;
    int target = std::rand() % 1000;
    if (std::rand() % 4 != 0) {
      pcsr.add_edge(src, target, i);
      ASSERT_TRUE(pcsr.edge_exists(src, target))
          << "Add: " << src << " " << target;
    } else {
      pcsr.remove_edge(src, target);
      ASSERT_FALSE(pcsr.edge_exists(src, target))
          << "Delete: " << src << " " << target;
    }
    // Check whether all locks were released
    for (uint32_t j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
      ASSERT_TRUE(pcsr.edges.node_locks[j]->lockable())
          << "Current iteration: " << i << " lock id: " << j;
    }
    ASSERT_TRUE(pcsr.edges.global_lock->lockable());
  }
}

TEST_P(DataStructureTest, add_remove_edge_random_2E4_par) {
  PCSR pcsr(1000, 1000, GetParam(), 0);
  constexpr int edge_count = 2E5;
#pragma omp parallel
  {
    pcsr.edges.global_lock->registerThread();
#pragma omp for nowait
    for (int i = 1; i < edge_count + 1; ++i) {
      int src = std::rand() % 1000;
      int target = std::rand() % 1000;
      if (std::rand() % 4 != 0) {
        pcsr.add_edge(src, target, i);
        EXPECT_TRUE(pcsr.edge_exists(src, target))
            << "Add: " << src << " " << target;
      } else {
        pcsr.remove_edge(src, target);
        EXPECT_FALSE(pcsr.edge_exists(src, target))
            << "Delete: " << src << " " << target;
      }
    }
    pcsr.edges.global_lock->unregisterThread();
  }

  // Check whether all locks were released
  for (uint32_t j = 0; j < pcsr.edges.N / pcsr.edges.logN; ++j) {
    EXPECT_TRUE(pcsr.edges.node_locks[j]->lockable()) << "Lock id: " << j;
  }
  EXPECT_TRUE(pcsr.edges.global_lock->lockable());
}

TEST_P(DataStructureTest, bfs_5E4) {
  PCSR pcsr(1000, 1000, GetParam(), 0);
  constexpr int edge_count = 5E4;
  for (int i = 1; i < edge_count + 1; ++i) {
    int src = std::rand() % 1000;
    int target = std::rand() % 1000;
    pcsr.add_edge(src, target, i);
  }

  // run BFS
  auto start = chrono::steady_clock::now();
  auto res = bfs(pcsr, 0);
  auto finish = chrono::steady_clock::now();
  EXPECT_EQ(res.size(), 1000);
  cout << "BFS time: "
       << chrono::duration_cast<chrono::milliseconds>(finish - start).count()
       << endl;
}

TEST_P(DataStructureTest, pagerank_5E4) {
  PCSR pcsr(1000, 1000, GetParam(), 0);
  constexpr int edge_count = 5E4;
  for (int i = 1; i < edge_count + 1; ++i) {
    int src = std::rand() % 1000;
    int target = std::rand() % 1000;
    pcsr.add_edge(src, target, i);
  }

  // run pagerank
  vector<float> weights(pcsr.get_n(), 1.0f);
  auto start = chrono::steady_clock::now();
  auto res = pagerank(pcsr, weights);
  auto finish = chrono::steady_clock::now();
  EXPECT_EQ(res.size(), 1000);
  cout << "Pagerank time: "
       << chrono::duration_cast<chrono::milliseconds>(finish - start).count()
       << endl;
}

INSTANTIATE_TEST_CASE_P(DataStructureTestSuite, DataStructureTest, Bool());