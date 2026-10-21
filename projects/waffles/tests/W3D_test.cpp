// W3D: W3DVector.
#include <gtest/gtest.h>

#include <WClasses/W3D.h>

#include <type_traits>

using namespace WClasses;

// Three numbers, copied as they are: the compiler's own copy, so a
// W3DVector can be copied with memcpy and kept in plain arrays.
static_assert(std::is_trivially_copyable_v<W3DVector>);

TEST(W3DVectorTest, CopiesAndSelfAssignment) {
  W3DVector a(1.0, -2.0, 3.5);
  W3DVector b(a);
  EXPECT_EQ(b.m_vals[1], -2.0);
  W3DVector c;
  c = a;
  EXPECT_EQ(c.m_vals[2], 3.5);
  W3DVector &same = c;
  c = same;
  EXPECT_EQ(c.m_vals[0], 1.0);
  EXPECT_EQ(c.m_vals[1], -2.0);
  EXPECT_EQ(c.m_vals[2], 3.5);
}
