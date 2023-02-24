/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
    Eric Moyer,
    anonymous contributors,

  to the public domain (http://creativecommons.org/publicdomain/zero/1.0/).

  Note that some moral obligations still exist in the absence of legal ones.
  For example, it would still be dishonest to deliberately misrepresent the
  origin of a work. Although we impose no legal requirements to obtain a
  license, it is beseeming for those who build on the works of others to
  give back useful improvements, or find a way to pay it forward. If
  you would like to cite us, a published paper about Waffles can be found
  at http://jmlr.org/papers/volume12/gashler11a/gashler11a.pdf. If you find
  our code to be useful, the Waffles team would love to hear how you use it.
*/

#include "GHolders.h"
#include "GError.h"

namespace GClasses {

#ifdef _DEBUG
GTempBufSentinel::GTempBufSentinel(void* pBuf)
: m_pBuf(pBuf)
{
	*(char*)pBuf = 'S';
}

GTempBufSentinel::~GTempBufSentinel()
{
	GAssert(*(char*)m_pBuf == 'S'); // buffer overrun!
}
#endif // _DEBUG

void GOverrunSentinel::Check()
{
	if(m_sentinel != 0x5e47143a)
		throw Ex("buffer overrun!");
}


void verboten()
{
	throw Ex("Tried to copy a holder. (The method that facilitates this should have been private to catch this at compile time.)");
}

void FileHolder::reset(FILE* pFile)
{
	if(m_pFile && pFile != m_pFile)
	{
		if(fclose(m_pFile) != 0)
			GAssert(false);
	}
	m_pFile = pFile;
}

} // namespace GClasses
