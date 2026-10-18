// Regression tests for bugs found in Waffles with clang-tidy (each failed
// before its fix).
#include <gtest/gtest.h>

#include <WClasses/GApp.h>
#include <WClasses/GBitTable.h>
#include <WClasses/GCluster.h>
#include <WClasses/GMath.h>
#include <WClasses/GMatrix.h>
#include <WClasses/GRand.h>
#include <WClasses/GSparseMatrix.h>
#include <WClasses/GTokenizer.h>
#include <WClasses/GVec.h>

#include <cstring>
#include <stdexcept>
#include <string>

using namespace GClasses;

// Self-assignment through a reference, so the compiler doesn't flag x = x
template <typename T> void assignToItself(T &x) {
  T &same = x;
  x = same;
}

// GBitTable's operator= freed its bits before copying from them.
TEST(SelfAssignmentTest, BitTableKeepsItsBits) {
  GBitTable table(100);
  table.set(3);
  table.set(64);
  assignToItself(table);
  EXPECT_TRUE(table.bit(3));
  EXPECT_TRUE(table.bit(64));
  EXPECT_FALSE(table.bit(4));
}

// GMatrix's operator= emptied the matrix before copying from it.
TEST(SelfAssignmentTest, MatrixKeepsItsData) {
  GMatrix m(2, 3);
  m[0][0] = 1.5;
  m[1][2] = -2.0;
  assignToItself(m);
  ASSERT_EQ(m.rows(), 2u);
  ASSERT_EQ(m.cols(), 3u);
  EXPECT_EQ(m[0][0], 1.5);
  EXPECT_EQ(m[1][2], -2.0);
}

// The normal approximation centres on n(n+1)/4, so a statistic equal to it
// gives p = 1. With n(n+1)/4 truncated (45 instead of 45.5 for n = 13) the
// p-value came out above 1.
TEST(WilcoxonTest, StatisticAtItsMeanGivesPOne) {
  EXPECT_NEAR(GMath::wilcoxonPValue(13, 13.0 * 14.0 / 4.0), 1.0, 1e-12);
  // (The statistic is the smaller rank sum, at or below the mean.)
  const double p = GMath::wilcoxonPValue(13, 30.0);
  EXPECT_GT(p, 0.0);
  EXPECT_LT(p, 1.0);
}

// Command-line numbers: "abc" used to become 0.
TEST(ArgReaderTest, PopDoubleRejectsNonNumbers) {
  char good[] = "2.5", sci[] = "-1e-3", bad[] = "abc", trailing[] = "3x";
  char *argv[] = {good, sci, bad, trailing};
  GArgReader args(4, argv);
  EXPECT_EQ(args.pop_double(), 2.5);
  EXPECT_EQ(args.pop_double(), -1e-3);
  EXPECT_THROW(args.pop_double(), std::exception);
  EXPECT_THROW(args.pop_double(), std::exception);
}

// Sparse k-means keeps a running mean per element over the rows that have
// it, but count / (count + 1) divided integers (always 0), so each update
// wiped the mean and left last value / count instead. Common elements were
// shrunk by their large counts and rare ones barely, dragging every centre
// toward rare elements. Here element 2 appears in just one row of each
// cluster: the old centres were dragged toward it and a row was stranded.
//
// The seeds are ones where the result depends only on the averaging. With
// some others (4, 7, 12, 15 of 1-20) the fixed version puts everything in
// one cluster: a separate weakness, as a cluster left with no members is
// never given a new starting point, which the wrong averages happened to
// avoid there.
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
  for (unsigned seed : {3u, 8u, 9u, 13u, 14u, 16u}) {
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

// GTensor's ARFF reader read each column's name and dropped it, so errors
// gave only the column number. They now name the column too.
namespace {
std::string tensorArffError(const char *arff) {
  GTensor t({1, 1});
  GTokenizer tok(arff, strlen(arff));
  try {
    t.parseArff(tok);
  } catch (const std::exception &e) {
    return e.what();
  }
  return "";
}
} // namespace

TEST(TensorArffTest, ErrorsNameTheColumn) {
  EXPECT_NE(tensorArffError("@relation r\n@attribute height REAL\n"
                            "@attribute colour {red,green}\n@data\n"
                            "1.5,purple\n")
                .find("for attribute colour (1)"),
            std::string::npos);
  EXPECT_NE(tensorArffError("@relation r\n@attribute height REAL\n"
                            "@attribute colour {red,green}\n@data\n"
                            "tall,red\n")
                .find("for attribute height (0)"),
            std::string::npos);
}

// parseArff copied the values out of its queue while i < queue.size(), but
// each pop shrank the queue, so it stopped halfway and left the rest of the
// tensor uninitialised.
TEST(TensorArffTest, LoadsEveryValue) {
  const char *arff = "@relation r\n@attribute a REAL\n@attribute b REAL\n"
                     "@data\n1,2\n3,4\n5,6\n";
  GTensor t({1, 1});
  GTokenizer tok(arff, strlen(arff));
  t.parseArff(tok);
  ASSERT_EQ(t.shape.size(), 2u);
  EXPECT_EQ(t.shape[0], 3u);
  EXPECT_EQ(t.shape[1], 2u);
  ASSERT_EQ(t.size(), 6u);
  for (size_t i = 0; i < 6; i++)
    EXPECT_EQ(t[i], i + 1.0) << "value " << i;
}

// With no @attribute lines, parseArff divided by the column count (0):
// undefined behaviour, which gives an empty tensor on arm64 and SIGFPE on
// x86. It's now an error.
TEST(TensorArffTest, NoColumnsIsAnError) {
  EXPECT_NE(tensorArffError("@relation r\n@data\n").find("no attributes"),
            std::string::npos);
}
