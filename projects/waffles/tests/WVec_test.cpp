// WTensor's ARFF reader.
#include <gtest/gtest.h>

#include <WClasses/WTokenizer.h>
#include <WClasses/WVec.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace WClasses;

// WTensor's ARFF reader read each column's name and dropped it, so errors
// gave only the column number. They now name the column too.
namespace {
std::string tensorArffError(const char *arff) {
  WTensor t({1, 1});
  WTokenizer tok(arff, strlen(arff));
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
  WTensor t({1, 1});
  WTokenizer tok(arff, strlen(arff));
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
  EXPECT_NE(tensorArffError("@relation r\n@data\n").find("no attributes"), std::string::npos);
}

// The same for WTensor's reader: "1.5abc" loaded as 1.5.
TEST(TensorArffTest, JunkAfterANumberIsAnError) {
  EXPECT_NE(tensorArffError("@relation r\n@attribute h REAL\n@data\n1.5abc\n")
                .find("for attribute h (0)"),
            std::string::npos);
}

// WVec isn't copied implicitly (use copy, which says how much) or made
// from a double (use fill): those are compile errors.
static_assert(!std::is_copy_constructible_v<WVec>);
static_assert(!std::is_copy_assignable_v<WVec>);
static_assert(!std::is_constructible_v<WVec, double>);
static_assert(std::is_constructible_v<WVec, size_t>);

// Returning a WVec by value moves it: operator+ and the others used to rely
// on the compiler skipping a copy, the copy constructor being one that threw.
TEST(VecMoveTest, ReturnedByValueAndMoved) {
  const WVec a({1.0, 2.0}), b({10.0, 20.0});
  WVec sum = a + b;
  ASSERT_EQ(sum.size(), 2u);
  EXPECT_EQ(sum[1], 22.0);
  const double *buffer = sum.data();
  WVec moved(std::move(sum));
  EXPECT_EQ(moved.data(), buffer); // taken over, not copied
  EXPECT_EQ(moved[0], 11.0);
  // (If sum kept the buffer as well, both would free it at the end of the
  // test, and the test would crash.)
  EXPECT_TRUE(std::is_nothrow_move_constructible_v<WVec>);
}

// A WVecWrapper copy wraps the same memory (it used to throw, through
// WVec's copy constructor). Assigning one is a compile error: setData or
// copy says which is meant.
TEST(VecWrapperTest, CopyWrapsTheSameMemory) {
  WVec v({1.0, 2.0, 3.0});
  WVecWrapper w(v, 1, 2);
  WVecWrapper copy(w);
  EXPECT_EQ(copy.data(), v.data() + 1);
  ASSERT_EQ(copy.size(), 2u);
  copy[0] = 9.0;
  EXPECT_EQ(v[1], 9.0);
  static_assert(!std::is_copy_assignable_v<WVecWrapper>);
}

// A copied tensor wraps the same buffer and has the same shape, and doesn't
// own the buffer (the original does).
TEST(TensorTest, CopyWrapsTheSameBuffer) {
  WTensor t({2, 3});
  t.fill(1.5);
  {
    WTensor copy(t);
    EXPECT_EQ(copy.data(), t.data());
    EXPECT_EQ(copy.size(), 6u);
    ASSERT_EQ(copy.shape.size(), 2u);
    EXPECT_EQ(copy.shape[0], 2u);
    EXPECT_EQ(copy.shape[1], 3u);
    EXPECT_EQ(copy.pHolder, nullptr);
    copy[4] = 9.0;
  }
  EXPECT_EQ(t[4], 9.0); // and the buffer outlives the copy
}
