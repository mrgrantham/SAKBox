// WMatrix and WBitTable assignment, WCSVParser and WRelation::print.
#include <gtest/gtest.h>

#include <WClasses/WBitTable.h>
#include <WClasses/WMatrix.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace WClasses;

// Self-assignment through a reference, so the compiler doesn't flag x = x
template <typename T> void assignToItself(T &x) {
  T &same = x;
  x = same;
}

// WBitTable's operator= freed its bits before copying from them.
TEST(SelfAssignmentTest, BitTableKeepsItsBits) {
  WBitTable table(100);
  table.set(3);
  table.set(64);
  assignToItself(table);
  EXPECT_TRUE(table.bit(3));
  EXPECT_TRUE(table.bit(64));
  EXPECT_FALSE(table.bit(4));
}

// WMatrix's operator= emptied the matrix before copying from it.
TEST(SelfAssignmentTest, MatrixKeepsItsData) {
  WMatrix m(2, 3);
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
  WCSVParser parser;
  WMatrix m;
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
  WCSVParser parser;
  parser.setRealAttr(0);
  WMatrix m;
  parser.parse(m, csv, strlen(csv));
  ASSERT_EQ(m.rows(), 22u);
  EXPECT_EQ(m[20][0], 21.0);
  EXPECT_EQ(m[21][0], UNKNOWN_REAL_VALUE);
}

TEST(RelationPrintTest, DatesAreWrittenAsReal) {
  WArffRelation rel;
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
WMatrix loadArff(const std::string &arff) {
  WMatrix m;
  m.parseArff(arff.c_str(), arff.size());
  return m;
}
std::string valueText(const WMatrix &m, size_t row, size_t col) {
  std::ostringstream os;
  m.relation().printAttrValue(os, col, m[row][col]);
  return os.str();
}
} // namespace

TEST(ArffStringColumnTest, LoadsAsACategory) {
  WMatrix m = loadArff("@relation r\n@attribute name string\n@attribute h real\n"
                       "@data\nalice,1.5\nbob,1.8\nalice,1.6\n'carol',1.7\n"
                       "\"dave smith\",1.9\n?,2.0\n'alice',1.4\n");
  ASSERT_EQ(m.rows(), 7u);
  EXPECT_EQ(m.relation().valueCount(0), 4u); // alice, bob, carol, dave smith
  EXPECT_EQ(m[0][0], 0.0);
  EXPECT_EQ(m[1][0], 1.0);
  EXPECT_EQ(m[2][0], 0.0);                    // alice again
  EXPECT_EQ(m[6][0], 0.0);                    // 'alice' is alice
  EXPECT_EQ(m[5][0], UNKNOWN_DISCRETE_VALUE); // ? is missing
  EXPECT_EQ(valueText(m, 1, 0), "bob");
  EXPECT_EQ(valueText(m, 4, 0), "\"dave smith\""); // kept as written
  EXPECT_EQ(m[4][1], 1.9);                         // the number column is intact
}

TEST(ArffStringColumnTest, SavedAsACategoryAndLoadsBackTheSame) {
  WMatrix m = loadArff("@relation r\n@attribute name string\n@data\n"
                       "alice\nbob\n\"dave smith\"\nbob\n");
  std::ostringstream saved;
  m.print(saved);
  EXPECT_NE(saved.str().find("{alice,bob,\"dave smith\"}"), std::string::npos) << saved.str();
  WMatrix back = loadArff(saved.str());
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
  WMatrix m = loadArff(freeText);
  const std::string warning = testing::internal::GetCapturedStderr();
  EXPECT_NE(warning.find("string column 's' has 40 different values in 40 rows"), std::string::npos)
      << warning;
  EXPECT_EQ(m.relation().valueCount(0), 40u); // loaded anyway
}

// WArffStrings::refuse turns string columns away instead: loading throws,
// naming the column, and the matrix keeps what it held. Files without one
// load as usual.
TEST(ArffStringColumnTest, RefusedWhenAsked) {
  const std::string arff = "@relation r\n@attribute h real\n@attribute name string\n"
                           "@data\n1.5,alice\n";
  WMatrix m = loadArff("@relation before\n@attribute x real\n@data\n7\n8\n");
  try {
    m.parseArff(arff.c_str(), arff.size(), WArffStrings::refuse);
    FAIL() << "a string column was loaded";
  } catch (const std::exception &e) {
    EXPECT_NE(std::string(e.what()).find("Column 'name' is an ARFF string column"),
              std::string::npos)
        << e.what();
  }
  ASSERT_EQ(m.rows(), 2u); // unchanged
  EXPECT_EQ(m.cols(), 1u);
  EXPECT_EQ(m[1][0], 8.0);

  const std::string fine = "@relation r\n@attribute h real\n@attribute c {red,blue}\n"
                           "@data\n1.5,blue\n2.5,red\n";
  m.parseArff(fine.c_str(), fine.size(), WArffStrings::refuse);
  ASSERT_EQ(m.rows(), 2u);
  EXPECT_EQ(m[0][1], 1.0); // blue

  // The default is still to load it as a category
  m.parseArff(arff.c_str(), arff.size(), WArffStrings::asCategories);
  EXPECT_EQ(m.relation().valueCount(1), 1u);
}

// The same from a file, with loadArff
TEST(ArffStringColumnTest, RefusedFromAFile) {
  const char *tmp = getenv("TEST_TMPDIR");
  const std::string path = std::string(tmp ? tmp : ".") + "/strings.arff";
  {
    std::ofstream out(path);
    out << "@relation r\n@attribute name string\n@data\nalice\n";
  }
  WMatrix m;
  EXPECT_THROW(m.loadArff(path.c_str(), WArffStrings::refuse), std::exception);
  m.loadArff(path.c_str());
  EXPECT_EQ(m.rows(), 1u);
}

// ARFF numbers were checked only by their first character, so "1.5abc"
// loaded as 1.5 and "1.8m" as 1.8.
TEST(ArffNumberTest, JunkAfterANumberIsAnError) {
  for (const char *bad : {"1.5abc", "1.8m", "1.2.3", "nan"}) {
    const std::string arff = std::string("@relation r\n@attribute h real\n@data\n") + bad + "\n";
    WMatrix m;
    EXPECT_THROW(m.parseArff(arff.c_str(), arff.size()), std::exception) << bad;
  }
  const std::string ok = "@relation r\n@attribute h real\n@data\n1.5\n-.5\n2e3\n?\n";
  WMatrix m;
  m.parseArff(ok.c_str(), ok.size());
  ASSERT_EQ(m.rows(), 4u);
  EXPECT_EQ(m[0][0], 1.5);
  EXPECT_EQ(m[1][0], -0.5);
  EXPECT_EQ(m[2][0], 2000.0);
  EXPECT_EQ(m[3][0], UNKNOWN_REAL_VALUE);
}

// WArffRelation::parseValue took a number for a category value it didn't
// recognise as the value's position, without checking it was in range.
TEST(ArffNumberTest, CategoryNumberMustBeInRange) {
  WArffRelation rel;
  std::vector<const char *> values = {"red", "green", "blue"};
  rel.addAttribute("c", 3, &values);
  EXPECT_EQ(rel.parseValue(0, "green"), 1.0);
  EXPECT_EQ(rel.parseValue(0, "2"), 2.0);
  EXPECT_THROW(rel.parseValue(0, "7"), std::exception);
  EXPECT_THROW(rel.parseValue(0, "1x"), std::exception);
  rel.addAttribute("n", 0, nullptr);
  EXPECT_EQ(rel.parseValue(1, "2.5"), 2.5);
  EXPECT_THROW(rel.parseValue(1, "2.5abc"), std::exception);
}

// WMatrix::sort and sortPartial. Neither promises an order for rows with
// equal keys (std::ranges::sort isn't stable), and standard libraries break
// ties differently, so these check what is promised: the order of the keys,
// that each row moves whole, and that no row is lost or repeated.
namespace {
// The key each row of keyedRows() is given, from its id
double keyOf(size_t id) { return (double)((id * 37) % 10); }

// 100 rows of (key, id), with keys 0-9 repeated in a scrambled order
WMatrix keyedRows() {
  WMatrix m(100, 2);
  for (size_t i = 0; i < 100; i++) {
    m[i][0] = keyOf(i);
    m[i][1] = (double)i;
  }
  return m;
}

// Every id appears exactly once, still with its own key
void expectSameRows(const WMatrix &m) {
  std::vector<bool> seen(100, false);
  for (size_t i = 0; i < m.rows(); i++) {
    const size_t id = (size_t)m[i][1];
    ASSERT_LT(id, seen.size()) << "row " << i;
    EXPECT_FALSE(seen[id]) << "id " << id << " appears twice";
    seen[id] = true;
    EXPECT_EQ(m[i][0], keyOf(id)) << "row " << i << " was split";
  }
}
} // namespace

TEST(MatrixSortTest, SortsByAColumn) {
  WMatrix m = keyedRows();
  m.sort(0);
  for (size_t i = 1; i < m.rows(); i++)
    ASSERT_LE(m[i - 1][0], m[i][0]) << "row " << i;
  expectSameRows(m);
}

TEST(MatrixSortTest, SortPartialPutsTheRightRowInPlace) {
  WMatrix m = keyedRows();
  m.sortPartial(50, 0);
  EXPECT_EQ(m[50][0], 5.0);
  for (size_t i = 0; i < m.rows(); i++)
    EXPECT_TRUE(i < 50 ? m[i][0] <= 5.0 : m[i][0] >= 5.0) << "row " << i;
  expectSameRows(m);
}
