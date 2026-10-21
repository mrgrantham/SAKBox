/*
  SAKBox: WStemmer wraps the repo's libstemmer (Snowball). This file used to
  hold a copy of Martin Porter's C implementation of his stemmer
  (tartarus.org/~martin/PorterStemmer), which departs from the published
  algorithm in a few places; Snowball's "porter" follows it as published.
*/

#include "WStemmer.h"

#include "WError.h"

#include <cctype>
#include <libstemmer.h>

namespace WClasses
{

WStemmer::WStemmer()
: m_pStemmer(sb_stemmer_new("porter", "UTF_8"))
{
	if(!m_pStemmer)
		throw Ex("Failed to create the Porter stemmer");
}

WStemmer::~WStemmer()
{
	sb_stemmer_delete(m_pStemmer);
}

const char* WStemmer::getStem(const char* szWord, size_t nLen)
{
	// Lowercase (tolower takes an unsigned char: a negative char, such as a
	// UTF-8 byte, is undefined behaviour)
	m_word.resize(nLen);
	for(size_t n = 0; n < nLen; n++)
		m_word[n] = (char)tolower((unsigned char)szWord[n]);
	const sb_symbol* pStem = sb_stemmer_stem(m_pStemmer, (const sb_symbol*)m_word.data(), (int)nLen);
	if(!pStem)
		throw Ex("Out of memory while stemming");
	m_stem.assign((const char*)pStem, sb_stemmer_length(m_pStemmer));
	return m_stem.c_str();
}

} // namespace WClasses
