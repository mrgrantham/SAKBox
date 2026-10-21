// WHashTable.h's hash tables.
#include <gtest/gtest.h>

#include <WClasses/WHashTable.h>

using namespace WClasses;

namespace {
struct KeyNode : HashTableNode {
  int key;
  explicit KeyNode(int k) : key(k) {}
  size_t hash(size_t nBucketCount) override { return (size_t)key % nBucketCount; }
  bool equals(HashTableNode *pThat) override { return key == static_cast<KeyNode *>(pThat)->key; }
};
} // namespace

// get finds the stored node equal to the one given (and wrote it out
// through a void** cast of a HashTableNode**, which is now a typed lookup).
TEST(NodeHashTableTest, GetReturnsTheStoredEqualNode) {
  WNodeHashTable table(true, 16);
  KeyNode *seven = new KeyNode(7), *twentyThree = new KeyNode(23); // (both in bucket 7)
  table.add(seven);
  table.add(twentyThree);
  KeyNode likeSeven(7), likeTwentyThree(23), likeEight(8);
  EXPECT_EQ(table.get(&likeSeven), seven);
  EXPECT_EQ(table.get(&likeTwentyThree), twentyThree);
  EXPECT_EQ(table.get(&likeEight), nullptr);
}
