// WSparseClusterRecommender.
#include <gtest/gtest.h>

#include <WClasses/WDom.h>
#include <WClasses/WLearner.h>
#include <WClasses/WMatrix.h>
#include <WClasses/WRand.h>
#include <WClasses/WRecommender.h>

#include <new>

using namespace WClasses;

// The cluster recommender predicts a rating from the user's cluster, so
// it's only as good as the clustering: with sparse k-means recomputing just
// one centre it mostly mixed the two groups here.
TEST(SparseClusterRecommenderTest, PredictsFromTheUsersGroup) {
  // 20 users (alternating between two groups) rating 8 items: group A likes
  // items 0-3 and dislikes 4-7, group B the opposite. Users 0 and 1 haven't
  // rated item 0.
  WRand noise(7);
  WMatrix ratings(0, 3);
  for (size_t user = 0; user < 20; user++)
    for (size_t item = 0; item < 8; item++) {
      if (user < 2 && item == 0)
        continue;
      const bool likes = (item < 4) == (user % 2 == 0);
      WVec &row = ratings.newRow();
      row[0] = (double)user;
      row[1] = (double)item;
      row[2] = (likes ? 5.0 : 1.0) + noise.normal() * 0.3;
    }
  WSparseClusterRecommender rec(2);
  rec.train(ratings);
  EXPECT_GT(rec.predict(0, 0), 4.0); // group A: likes item 0
  EXPECT_LT(rec.predict(1, 0), 2.0); // group B: dislikes it
}

// A baseline recommender predicts each item's mean rating, and a saved one
// loads back predicting the same (it used to forget how many items it had,
// and predicted 0).
TEST(BaselineRecommenderTest, LoadsBackPredictingTheSame) {
  WMatrix ratings(0, 3);
  for (size_t user = 0; user < 3; user++)
    for (size_t item = 0; item < 3; item++) {
      WVec &row = ratings.newRow();
      row[0] = (double)user;
      row[1] = (double)item;
      row[2] = (double)(user + 2 * item);
    }
  WBaselineRecommender baseline;
  baseline.train(ratings);
  WDom doc;
  WLearnerLoader loader;
  // Loaded into zeroed memory, so a count left uninitialised reads as 0
  // every time rather than whatever was there before
  alignas(WBaselineRecommender) unsigned char memory[sizeof(WBaselineRecommender)] = {};
  auto *loaded = new (memory) WBaselineRecommender(baseline.serialize(&doc), loader);
  for (size_t item = 0; item < 3; item++) {
    EXPECT_EQ(baseline.predict(0, item), 1.0 + 2 * item) << item; // mean of users 0-2
    EXPECT_EQ(loaded->predict(0, item), baseline.predict(0, item)) << item;
  }
  loaded->~WBaselineRecommender();
}
