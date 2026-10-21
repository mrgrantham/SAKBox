// WStemmer: the Porter stemmer, from the repo's libstemmer (Snowball).
#include <gtest/gtest.h>

#include <WClasses/WStemmer.h>

#include <cstring>
#include <string>

using namespace WClasses;

namespace {
std::string stemOf(WStemmer &stemmer, const std::string &word) {
  return stemmer.getStem(word.data(), word.size());
}
} // namespace

// The examples in WStemmer.h
TEST(StemmerTest, DocumentedExamples) {
  WStemmer stemmer;
  const char *pairs[][2] = {
      {"cats", "cat"},        {"dogs", "dog"},          {"fries", "fri"},   {"fishes", "fish"},
      {"pies", "pi"},         {"lovingly", "lovingli"}, {"candy", "candi"}, {"babies", "babi"},
      {"bus", "bu"},          {"busses", "buss"},       {"women", "women"}, {"hasty", "hasti"},
      {"hastily", "hastili"}, {"fly", "fly"},           {"kisses", "kiss"}, {"goes", "goe"},
      {"brought", "brought"},
  };
  for (auto &p : pairs)
    EXPECT_EQ(stemOf(stemmer, p[0]), p[1]) << p[0];
  // Lowercased first
  EXPECT_EQ(stemOf(stemmer, "Kisses"), "kiss");
  // Bytes outside ASCII (UTF-8 here) are kept as they are
  EXPECT_EQ(stemOf(stemmer, "CAF\xC3\x89S"), "caf\xC3\x89");
}

// WStemmer is Snowball's "porter", the algorithm as published. Waffles
// used to carry Porter's own C version, which departs from it in a few
// places: these words, about 1 in 220 of a dictionary, stem differently.
TEST(StemmerTest, PublishedPorterAlgorithm) {
  WStemmer stemmer;
  EXPECT_EQ(stemOf(stemmer, "biology"), "biologi");       // was "biolog"
  EXPECT_EQ(stemOf(stemmer, "accessibly"), "accessibli"); // was "access"
  EXPECT_EQ(stemOf(stemmer, "as"), "a");                  // was "as"
  EXPECT_EQ(stemOf(stemmer, "troubling"), "troubl");
  EXPECT_EQ(stemOf(stemmer, "conditional"), "condit");
}

// A word of any length is stemmed whole. (The copied C code worked in a
// 64-byte buffer: longer words were cut to 63 letters, and a 64-letter one
// wrote its '\0' past the end.)
TEST(StemmerTest, LongWordsAreStemmedWhole) {
  WStemmer stemmer;
  EXPECT_EQ(stemOf(stemmer, std::string(100, 'b')), std::string(100, 'b')); // no suffix
  // (Porter strips -ness only from a stem with a vowel in it)
  const std::string stem = "a" + std::string(99, 'b');
  EXPECT_EQ(stemOf(stemmer, stem + "ness"), stem);
}

// Text isn't always valid UTF-8: such bytes are stemmed without trouble.
TEST(StemmerTest, BytesThatArentUtf8) {
  WStemmer stemmer;
  const std::string bad = "caf\xE9s\xFF\x80";
  EXPECT_NO_THROW(stemOf(stemmer, bad));
  EXPECT_EQ(stemOf(stemmer, ""), "");
}
