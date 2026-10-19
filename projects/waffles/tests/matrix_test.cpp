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

// ARFF string columns used to load as a column of zeros typed "real", the
// text thrown away. Each distinct text is now one value of a category.
namespace {
GMatrix loadArff(const std::string &arff) {
  GMatrix m;
  m.parseArff(arff.c_str(), arff.size());
  return m;
}
std::string valueText(const GMatrix &m, size_t row, size_t col) {
  std::ostringstream os;
  m.relation().printAttrValue(os, col, m[row][col]);
  return os.str();
}
} // namespace

TEST(ArffStringColumnTest, LoadsAsACategory) {
  GMatrix m = loadArff("@relation r\n@attribute name string\n@attribute h real\n"
                       "@data\nalice,1.5\nbob,1.8\nalice,1.6\n'carol',1.7\n"
                       "\"dave smith\",1.9\n?,2.0\n'alice',1.4\n");
  ASSERT_EQ(m.rows(), 7u);
  EXPECT_EQ(m.relation().valueCount(0), 4u); // alice, bob, carol, dave smith
  EXPECT_EQ(m[0][0], 0.0);
  EXPECT_EQ(m[1][0], 1.0);
  EXPECT_EQ(m[2][0], 0.0);                   // alice again
  EXPECT_EQ(m[6][0], 0.0);                   // 'alice' is alice
  EXPECT_EQ(m[5][0], UNKNOWN_DISCRETE_VALUE); // ? is missing
  EXPECT_EQ(valueText(m, 1, 0), "bob");
  EXPECT_EQ(valueText(m, 4, 0), "\"dave smith\""); // kept as written
  EXPECT_EQ(m[4][1], 1.9);                         // the number column is intact
}

TEST(ArffStringColumnTest, SavedAsACategoryAndLoadsBackTheSame) {
  GMatrix m = loadArff("@relation r\n@attribute name string\n@data\n"
                       "alice\nbob\n\"dave smith\"\nbob\n");
  std::ostringstream saved;
  m.print(saved);
  EXPECT_NE(saved.str().find("{alice,bob,\"dave smith\"}"), std::string::npos)
      << saved.str();
  GMatrix back = loadArff(saved.str());
  ASSERT_EQ(back.rows(), 4u);
  for (size_t i = 0; i < 4; i++)
    EXPECT_EQ(valueText(back, i, 0), valueText(m, i, 0)) << "row " << i;
}

TEST(ArffStringColumnTest, WarnsWhenItLooksLikeFreeText) {
  std::string labels = "@relation r\n@attribute s string\n@data\n";
  std::string freeText = labels;
  for (int i = 0; i < 40; i++) {
    labels += (i % 3 == 0 ? "red\n" : i % 3 == 1 ? "green\n" : "blue\n");
    freeText += "comment" + std::to_string(i) + "\n";
  }
  testing::internal::CaptureStderr();
  loadArff(labels);
  EXPECT_EQ(testing::internal::GetCapturedStderr(), "");
  testing::internal::CaptureStderr();
  GMatrix m = loadArff(freeText);
  const std::string warning = testing::internal::GetCapturedStderr();
  EXPECT_NE(warning.find("string column 's' has 40 different values in 40 rows"),
            std::string::npos)
      << warning;
  EXPECT_EQ(m.relation().valueCount(0), 40u); // loaded anyway
}
