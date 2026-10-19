// GStemmer and GVocabulary.
#include <gtest/gtest.h>

#include <WClasses/GStemmer.h>

#include <cstring>
#include <string>

using namespace GClasses;

namespace {
std::string stemOf(GStemmer &stemmer, const std::string &word) {
  return stemmer.getStem(word.data(), word.size());
}
} // namespace

// The examples in GStemmer.h
TEST(StemmerTest, DocumentedExamples) {
  GStemmer stemmer;
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

// A word of 64 letters or more filled the 64-byte buffer, and the '\0'
// after the stem was written one byte past its end.
TEST(StemmerTest, LongWordsStayInTheBuffer) {
  struct Guarded {
    GStemmer stemmer;
    char after[8];
  };
  Guarded g;
  memset(g.after, 'x', sizeof(g.after));
  const std::string word(100, 'b'); // no suffix to strip
  const std::string stem = stemOf(g.stemmer, word);
  EXPECT_EQ(stem, std::string(GSTEMMER_MAX_WORD_SIZE - 1, 'b'));
  EXPECT_EQ(g.after[0], 'x');
}
