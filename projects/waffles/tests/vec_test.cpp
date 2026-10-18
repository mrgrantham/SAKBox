// GTensor's ARFF reader.
#include <gtest/gtest.h>

#include <WClasses/GTokenizer.h>
#include <WClasses/GVec.h>

#include <cstring>
#include <stdexcept>
#include <string>

using namespace GClasses;

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
