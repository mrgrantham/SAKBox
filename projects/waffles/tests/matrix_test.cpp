// GMatrix and GBitTable assignment, GCSVParser and GRelation::print.
#include <gtest/gtest.h>

#include <WClasses/GBitTable.h>
#include <WClasses/GMatrix.h>

#include <cstring>
#include <sstream>
#include <string>
#include <vector>

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

TEST(CsvParserTest, EmptyAndQuestionMarkAreMissing) {
  // Column 0 is numbers, column 1 is categories; rows 2 and 3 leave both
  // out, one way each
  const char *csv = "1.5,red\n2.5,blue\n,\n?,?\n3.5,red\n";
  GCSVParser parser;
  GMatrix m;
  parser.parse(m, csv, strlen(csv));
  ASSERT_EQ(m.rows(), 5u);
  ASSERT_EQ(m.relation().valueCount(0), 0u); // numbers
  ASSERT_EQ(m.relation().valueCount(1), 2u); // red, blue
  EXPECT_EQ(m[0][0], 1.5);
  EXPECT_EQ(m[2][0], UNKNOWN_REAL_VALUE);
  EXPECT_EQ(m[3][0], UNKNOWN_REAL_VALUE);
  EXPECT_EQ(m[4][0], 3.5);
  EXPECT_EQ(m[0][1], m[4][1]); // both red
  EXPECT_NE(m[0][1], m[1][1]);
  EXPECT_EQ(m[2][1], UNKNOWN_DISCRETE_VALUE);
  EXPECT_EQ(m[3][1], UNKNOWN_DISCRETE_VALUE);
}

TEST(CsvParserTest, InvalidNumberInANumberColumnIsMissing) {
  const char *csv = "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n"
                    "17\n18\n19\n20\n21\nabc\n";
  GCSVParser parser;
  parser.setRealAttr(0);
  GMatrix m;
  parser.parse(m, csv, strlen(csv));
  ASSERT_EQ(m.rows(), 22u);
  EXPECT_EQ(m[20][0], 21.0);
  EXPECT_EQ(m[21][0], UNKNOWN_REAL_VALUE);
}

TEST(RelationPrintTest, DatesAreWrittenAsReal) {
  GArffRelation rel;
  rel.setName("r");
  std::vector<const char *> values = {"x", "y"};
  rel.addAttribute("n", 0, nullptr);
  rel.addAttribute("c", 2, &values);
  rel.addAttribute("s", (size_t)-1, nullptr); // string
  rel.addAttribute("d", (size_t)-2, nullptr); // date
  std::ostringstream os;
  rel.print(os);
  EXPECT_EQ(os.str(), "@RELATION r\n\n@ATTRIBUTE n real\n@ATTRIBUTE c {x,y}\n"
                      "@ATTRIBUTE s string\n@ATTRIBUTE d real\n\n@DATA\n");
}
