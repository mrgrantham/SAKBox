#ifndef __GSTEMMER_H__
#define __GSTEMMER_H__

#include <cstddef>
#include <string>

struct sb_stemmer;

namespace WClasses
{

/// This class just wraps the Porter Stemmer (Snowball's "porter", from the
/// repo's libstemmer: the algorithm as published).
/// It finds the stems of words.  Examples:
/// "cats"->"cat"
/// "dogs"->"dog"
/// "fries"->"fri"
/// "fishes"->"fish"
/// "pies"->"pi"
/// "lovingly"->"lovingli"
/// "candy"->"candi"
/// "babies"->"babi"
/// "bus"->"bu"
/// "busses"->"buss"
/// "women"->"women"
/// "hasty"->"hasti"
/// "hastily"->"hastili"
/// "fly"->"fly"
/// "kisses"->"kiss"
/// "goes"->"goe"
/// "brought"->"brought"
/// As you can see the stems aren't always real words, but
/// that's okay as long as it produces the same stem for words
/// that have the same etymological roots. Even then it still
/// isn't perfect (notice it got "bus" wrong), but it should
/// still improve analysis somewhat in many cases.
class WStemmer
{
protected:
	sb_stemmer* m_pStemmer;
	std::string m_word;
	std::string m_stem;

public:
	WStemmer();
	~WStemmer();
	WStemmer(const WStemmer&) = delete;
	WStemmer& operator=(const WStemmer&) = delete;

	/// Pass in a word, as UTF-8 (you don't need to lowercase or null-terminate
	/// it), and this will return its stem. The buffer it returns is only valid
	/// until the next time you call getStem.
	const char* getStem(const char* szWord, size_t nLen);
};

} // namespace WClasses

#endif // __GSTEMMER_H__
