// Inplace::Vector behaves as std::vector does for what the games use, holds
// up to its room without allocating, copies as one block (trivially, for
// trivial items), compares by its items, and stops the program when pushed
// past its room
#include <Inplace/Vector.hpp>

#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

using Inplace::Vector;

TEST(VectorTest, ActsAsAVector) {
  Vector<int, 8> list{3, 1, 4};
  EXPECT_EQ(list.size(), 3u);
  list.push_back(1);
  list.emplace_back(5);
  EXPECT_EQ((std::vector<int>(list.begin(), list.end())), (std::vector<int>{3, 1, 4, 1, 5}));
  list.insert(list.begin() + 1, 9);
  list.insert(list.end(), 2, 7);
  EXPECT_EQ((std::vector<int>(list.begin(), list.end())),
            (std::vector<int>{3, 9, 1, 4, 1, 5, 7, 7}));
  list.erase(list.begin());
  list.erase(list.begin() + 1, list.begin() + 3);
  EXPECT_EQ((std::vector<int>(list.begin(), list.end())), (std::vector<int>{9, 1, 5, 7, 7}));
  EXPECT_EQ(Inplace::erase(list, 7), 2u);
  EXPECT_EQ(Inplace::erase_if(list, [](int x) { return x > 8; }), 1u);
  EXPECT_EQ((std::vector<int>(list.begin(), list.end())), (std::vector<int>{1, 5}));
  EXPECT_EQ(list.front(), 1);
  EXPECT_EQ(list.back(), 5);
  list.pop_back();
  list.resize(3, 2);
  EXPECT_EQ((std::vector<int>(list.begin(), list.end())), (std::vector<int>{1, 2, 2}));
  const std::vector<int> from{6, 6, 6, 6};
  list.assign(from.begin(), from.end());
  EXPECT_EQ(list.size(), 4u);
  list.clear();
  EXPECT_TRUE(list.empty());
  EXPECT_EQ(list.capacity(), 8u);
}

TEST(VectorTest, ComparesByItemsAndCopiesAsABlock) {
  Vector<int, 8> a{1, 2}, b{1, 2, 3};
  b.pop_back();
  EXPECT_EQ(a, b); // (what's left in b's room past its items doesn't count)
  b.push_back(4);
  EXPECT_NE(a, b);
  static_assert(std::is_trivially_copyable_v<Vector<int, 8>>);
  const Vector<int, 8> copy = b;
  EXPECT_EQ(copy, b);
}

// (a helper, as a macro's argument can't hold a braced list's commas)
void pushPastTheRoom() {
  Vector<int, 2> full{1, 2};
  full.push_back(3);
}

TEST(VectorTest, PushingPastItsRoomStops) {
  EXPECT_DEATH(pushPastTheRoom(), "more than its 2 items");
}
