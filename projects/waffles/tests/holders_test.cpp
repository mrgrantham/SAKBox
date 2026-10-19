// GHolders: Holder and ArrayHolder.
#include <gtest/gtest.h>

#include <WClasses/GHolders.h>

#include <type_traits>

using namespace GClasses;

// A holder deletes what it holds, so copying one would delete it twice.
// Copying is refused at compile time.
static_assert(!std::is_copy_constructible_v<Holder<int>>);
static_assert(!std::is_copy_assignable_v<Holder<int>>);
static_assert(!std::is_copy_constructible_v<ArrayHolder<int>>);
static_assert(!std::is_copy_assignable_v<ArrayHolder<int>>);

namespace {
struct Counted {
  static int alive;
  Counted() { alive++; }
  ~Counted() { alive--; }
};
int Counted::alive = 0;
} // namespace

TEST(HolderTest, DeletesWhatItHolds) {
  {
    Holder<Counted> h(new Counted());
    EXPECT_EQ(Counted::alive, 1);
    h.reset(h.get()); // the same object: kept
    EXPECT_EQ(Counted::alive, 1);
    h.reset(new Counted()); // a new one: the old one is deleted
    EXPECT_EQ(Counted::alive, 1);
    delete h.release();
    EXPECT_EQ(Counted::alive, 0);
    h.reset(new Counted());
  }
  EXPECT_EQ(Counted::alive, 0);
  {
    ArrayHolder<Counted> a(new Counted[3]);
    EXPECT_EQ(Counted::alive, 3);
    a.reset(new Counted[2]);
    EXPECT_EQ(Counted::alive, 2);
  }
  EXPECT_EQ(Counted::alive, 0);
}
