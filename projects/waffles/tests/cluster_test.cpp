// GKMeansSparse.
#include <gtest/gtest.h>

#include <WClasses/GCluster.h>
#include <WClasses/GRand.h>
#include <WClasses/GSparseMatrix.h>

#include <vector>

using namespace GClasses;

// Sparse k-means keeps a running mean per element over the rows that have
// it, but count / (count + 1) divided integers (always 0), so each update
// wiped the mean and left last value / count instead. Common elements were
// shrunk by their large counts and rare ones barely, dragging every centre
// toward rare elements. Here element 2 appears in just one row of each
// cluster: the old centres were dragged toward it and a row was stranded.
//
// For some seeds (4, 7, 12, 15) both starting centres point the same way,
// which cosine similarity can't tell apart: every row tied, went to the
// first, and the second cluster stayed empty and never moved. An empty
// cluster is now restarted from another cluster's worst-fitting row.
TEST(KMeansSparseTest, RareElementsDontDragTheCentres) {
  GSparseMatrix data(8, 3);
  for (size_t i = 0; i < 4; i++) { // cluster A: mostly element 0
    data.set(i, 0, 1.0);
    data.set(i, 1, 0.1);
  }
  for (size_t i = 4; i < 8; i++) { // cluster B: mostly element 1
    data.set(i, 0, 0.1);
    data.set(i, 1, 1.0);
  }
  data.set(3, 2, 1.0); // a rare element, once in each cluster
  data.set(7, 2, 1.0);
  for (unsigned seed = 1; seed <= 20; seed++) {
    SCOPED_TRACE(testing::Message() << "seed " << seed);
    GRand rand(seed);
    GKMeansSparse kmeans(2, &rand);
    kmeans.cluster(&data);
    for (size_t i = 1; i < 4; i++)
      EXPECT_EQ(kmeans.whichCluster(i), kmeans.whichCluster(0)) << "row " << i;
    for (size_t i = 5; i < 8; i++)
      EXPECT_EQ(kmeans.whichCluster(i), kmeans.whichCluster(4)) << "row " << i;
    EXPECT_NE(kmeans.whichCluster(0), kmeans.whichCluster(4));
  }
}

// Recomputing a centre found its rows through a pointer that only moved on
// for rows in that cluster, so it stuck at the first row of another
// cluster: only the first row's cluster was ever recomputed, from the rows
// before that, and every other centre stayed where it started. Here 30
// users in 3 taste groups (rows in mixed order, each rating 2/3 of the
// items, with noise): the old code found the groups for almost no seeds.
TEST(KMeansSparseTest, EveryCentreFollowsItsRows) {
  GRand noise(12345);
  GSparseMatrix data(30, 9);
  std::vector<size_t> group(30);
  for (size_t r = 0; r < 30; r++) {
    const size_t row = (r * 7) % 30, g = r % 3;
    group[row] = g;
    for (size_t item = 0; item < 9; item++)
      if (noise.uniform() >= 0.33) // group g likes items 3g to 3g + 2
        data.set(row, item, (item / 3 == g ? 4.0 : 1.0) + noise.normal() * 0.5);
  }
  size_t found = 0;
  for (unsigned seed = 1; seed <= 50; seed++) {
    GRand rand(seed);
    GKMeansSparse kmeans(3, &rand);
    kmeans.cluster(&data);
    // Found: rows share a cluster exactly when they share a group
    bool same = true;
    for (size_t i = 0; i < 30; i++)
      for (size_t j = 0; j < 30; j++)
        if ((group[i] == group[j]) !=
            (kmeans.whichCluster(i) == kmeans.whichCluster(j)))
          same = false;
    found += same;
  }
  // k-means can settle on a worse answer from a poor start, so not all 50
  EXPECT_GE(found, 45u);
}
