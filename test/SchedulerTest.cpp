/**
 * @file SchedulerTest.cpp
 * @author Christian Menges
 */

#include <map>
#include <numeric>

#include "SchedulerTest.h"

TEST_F(SchedulerTest, lookupTableCreation) {
  for (int d = 1; d <= 8; ++d) {
    for (int t = 1; t <= 256; ++t) {

      int minNumThreads = t / d;
      int threshold = t % d;
      std::vector<int> threadToDomain(t);
      std::vector<int> firstThreadDomain(d, 0);
      std::vector<int> numThreadsDomain(d);
      int counter = 0;
      int currentDomain = 0;

      for (int i = 0; i < t; i++) {
        threadToDomain[i] = currentDomain;
        counter++;
        if (counter == minNumThreads + (currentDomain < threshold)) {
          numThreadsDomain[currentDomain] = counter;
          firstThreadDomain[currentDomain] = i - counter + 1;
          counter = 0;
          currentDomain++;
        }
      }

      ASSERT_EQ(std::accumulate(numThreadsDomain.cbegin(),
                                numThreadsDomain.cend(), 0),
                t);
      ASSERT_EQ(firstThreadDomain.size(), d);
      ASSERT_EQ(firstThreadDomain[0], 0);
      std::set<int> domains;
      for (int i = 0; i < t; ++i) {

        const int domain = threadToDomain[i];

        ASSERT_GT(d, domain) << "#domains: " << d << " #threads: " << t
                             << " current thread: " << i;

        domains.insert(domain);
      }
      ASSERT_EQ(std::min(t, d), domains.size())
          << "#domains: " << d << " #threads: " << t;
      std::set<int> differentDomainSizes;
      for (const auto it : numThreadsDomain) {
        differentDomainSizes.insert(it);
      }
      ASSERT_LT(0, differentDomainSizes.size());
      ASSERT_GE(2, differentDomainSizes.size());
    }
  }
}