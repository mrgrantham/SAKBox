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

#include "WBlob.h"
#include "WBits.h"

using namespace WClasses;
using std::string;

WBlobIncoming::WBlobIncoming()
{
	m_pBuffer = nullptr;
	m_nBufferSize = 0;
	m_nBufferPos = 0;
	m_bDeleteBuffer = false;
}

WBlobIncoming::WBlobIncoming(unsigned char* pBuffer, size_t nSize, bool bDeleteBuffer)
{
	m_bDeleteBuffer = false;
	setBlob(pBuffer, nSize, bDeleteBuffer);
}

WBlobIncoming::~WBlobIncoming()
{
	if(m_bDeleteBuffer)
		delete[] m_pBuffer;
}

void WBlobIncoming::setBlob(unsigned char* pBuffer, size_t nSize, bool bDeleteBuffer)
{
	if(m_bDeleteBuffer)
	{
		WAssert(pBuffer != m_pBuffer); // You gave me ownership of the same buffer twice
		delete(m_pBuffer);
	}
	m_pBuffer = pBuffer;
	m_nBufferSize = nSize;
	m_nBufferPos = 0;
	m_bDeleteBuffer = bDeleteBuffer;
}

void WBlobIncoming::get(unsigned char* pData, size_t nSize)
{
	if(m_nBufferSize - m_nBufferPos < nSize)
		throw Ex("WBlobIncoming blob is too small to contain the expected data");
	memcpy(pData, &m_pBuffer[m_nBufferPos], nSize);
	m_nBufferPos += nSize;
}

void WBlobIncoming::get(wchar_t* pwc)
{
	get((unsigned char*)pwc, sizeof(wchar_t));
#ifdef WINDOWS
	WAssert(sizeof(wchar_t) == 2);
	*pwc = WBits::littleEndianToN16((unsigned short)*pwc);
#else
	WAssert(sizeof(wchar_t) == 4);
	*pwc = WBits::littleEndianToN32(*pwc);
#endif
}

void WBlobIncoming::get(char* pc)
{
	get((unsigned char*)pc, sizeof(char));
}

void WBlobIncoming::get(int* pn)
{
	get((unsigned char*)pn, sizeof(int));
	*pn = WBits::littleEndianToN32(*pn);
}

void WBlobIncoming::get(unsigned int* pui)
{
	get((unsigned char*)pui, sizeof(unsigned int));
	*pui = WBits::littleEndianToN32(*pui);
}

void WBlobIncoming::get(unsigned long long* pull)
{
	get((unsigned char*)pull, sizeof(unsigned long long));
	*pull = WBits::littleEndianToN64(*pull);
}

void WBlobIncoming::get(unsigned char* puc)
{
	get(puc, sizeof(unsigned char));
}

void WBlobIncoming::get(float* pf)
{
	get((unsigned char*)pf, sizeof(float));
	*pf = WBits::littleEndianToR32(*pf);
}

void WBlobIncoming::get(double* pd)
{
	get((unsigned char*)pd, sizeof(double));
	*pd = WBits::littleEndianToR64(*pd);
}

void WBlobIncoming::get(string* pString)
{
	unsigned int nLen;
	get(&nLen);
	if(m_nBufferSize - m_nBufferPos < nLen)
		throw Ex("WBlobIncoming blob is too small to contain the expected data");
	pString->assign((const char*)&m_pBuffer[m_nBufferPos], nLen);
	m_nBufferPos += nLen;
}

void WBlobIncoming::peek(size_t nIndex, unsigned char* pData, size_t nSize)
{
	if(nIndex + nSize > m_nBufferSize)
		throw Ex("WBlobIncoming peek out of range");
	memcpy(pData, &m_pBuffer[nIndex], nSize);
}

// ----------------------------------------------------------------------

WBlobOutgoing::WBlobOutgoing(size_t nBufferSize, bool bOkToResizeBuffer)
{
	if(nBufferSize > 0)
		m_pBuffer = new unsigned char[nBufferSize];
	else
		m_pBuffer = nullptr;
	m_nBufferSize = nBufferSize;
	m_nBufferPos = 0;
	m_bOkToResizeBuffer = bOkToResizeBuffer;
}

WBlobOutgoing::~WBlobOutgoing()
{
	delete[] m_pBuffer;
}

void WBlobOutgoing::resizeBuffer(size_t nRequiredSize)
{
	if(m_bOkToResizeBuffer)
	{
		size_t nNewSize = std::max((size_t)3 * m_nBufferSize, std::max((size_t)1024, nRequiredSize));
		unsigned char* pNewBuffer = new unsigned char[nNewSize];
		memcpy(pNewBuffer, m_pBuffer, m_nBufferPos);
		delete[] m_pBuffer;
		m_pBuffer = pNewBuffer;
		m_nBufferSize = nNewSize;
	}
	else
		throw Ex("WBlobOutgoing buffer too small to hold blob");
}

void WBlobOutgoing::add(const unsigned char* pData, size_t nSize)
{
	if(m_nBufferSize - m_nBufferPos < nSize)
		resizeBuffer(m_nBufferPos + nSize);
	memcpy(&m_pBuffer[m_nBufferPos], pData, nSize);
	m_nBufferPos += nSize;
}

void WBlobOutgoing::add(const wchar_t wc)
{
#ifdef WINDOWS
	WAssert(sizeof(wchar_t) == 2);
	wchar_t tmp = WBits::n16ToLittleEndian((unsigned short)wc);
	add((const unsigned char*)&tmp, sizeof(wchar_t));
#else
	WAssert(sizeof(wchar_t) == 4);
	wchar_t tmp = WBits::n32ToLittleEndian(wc);
	add((const unsigned char*)&tmp, sizeof(wchar_t));
#endif
}

void WBlobOutgoing::add(const char c)
{
	add((const unsigned char*)&c, sizeof(char));
}

void WBlobOutgoing::add(const int n)
{
	int i = WBits::n32ToLittleEndian(n);
	add((const unsigned char*)&i, sizeof(int));
}

void WBlobOutgoing::add(const unsigned int n)
{
	unsigned int i = WBits::n32ToLittleEndian(n);
	add((const unsigned char*)&i, sizeof(unsigned int));
}

void WBlobOutgoing::add(const unsigned long long n)
{
	unsigned long long i = WBits::n64ToLittleEndian(n);
	add((const unsigned char*)&i, sizeof(unsigned long long));
}

void WBlobOutgoing::add(const unsigned char uc)
{
	add((const unsigned char*)&uc, sizeof(unsigned char));
}

void WBlobOutgoing::add(const float f)
{
	float f2 = WBits::r32ToLittleEndian(f);
	add((const unsigned char*)&f2, sizeof(float));
}

void WBlobOutgoing::add(const double d)
{
	double d2 = WBits::r64ToLittleEndian(d);
	add((const unsigned char*)&d2, sizeof(double));
}

void WBlobOutgoing::add(const char* szString)
{
	size_t nLen = strlen(szString);
	add((unsigned int)nLen);
	add((const unsigned char*)szString, nLen);
}

void WBlobOutgoing::poke(size_t nIndex, const unsigned char* pData, size_t nSize)
{
	if(nIndex + nSize > m_nBufferPos)
		throw Ex("WBlobOutgoing poke out of range");
	memcpy(&m_pBuffer[nIndex], pData, nSize);
}

void WBlobOutgoing::poke(size_t nIndex, const int n)
{
	int tmp = WBits::n32ToLittleEndian(n);
	poke(nIndex, (const unsigned char*)&tmp, sizeof(int));
}








WBlobQueue::WBlobQueue()
: m_pBuffer(nullptr), m_bufferAllocation(0), m_bufferBytes(0), m_pPos(nullptr), m_bytesRemaining(0)
{
}

WBlobQueue::~WBlobQueue()
{
	delete[] m_pBuffer;
}

void WBlobQueue::reallocBuffer(size_t newSize)
{
	char* pNewBuffer = new char[newSize];
	memcpy(pNewBuffer, m_pBuffer, m_bufferBytes);
	m_bufferAllocation = newSize;
	delete[] m_pBuffer;
	m_pBuffer = pNewBuffer;
}

void WBlobQueue::enqueue(const char* pBlob, size_t len)
{
	if(m_bytesRemaining > 0)
		throw Ex("There are still bytes remainging to be processed. You should call dequeue until it returns NULL");
	m_pPos = pBlob;
	m_bytesRemaining = len;
}

const char* WBlobQueue::dequeue(size_t len)
{
	if(m_bufferBytes > 0)
	{
		if(m_bufferAllocation < len)
			reallocBuffer(len);
		size_t bytesNeeded = len - m_bufferBytes;
		if(m_bytesRemaining >= bytesNeeded)
		{
			memcpy(m_pBuffer + m_bufferBytes, m_pPos, bytesNeeded);
			m_pPos += bytesNeeded;
			m_bytesRemaining -= bytesNeeded;
			m_bufferBytes = 0;
			return m_pBuffer;
		}
		else
		{
			memcpy(m_pBuffer + m_bufferBytes, m_pPos, m_bytesRemaining);
			m_bufferBytes += m_bytesRemaining;
			m_bytesRemaining = 0;
			return nullptr;
		}
	}
	else
	{
		if(m_bytesRemaining >= len)
		{
			const char* pChunk = m_pPos;
			m_pPos += len;
			m_bytesRemaining -= len;
			return pChunk;
		}
		else
		{
			if(m_bufferAllocation < len)
				reallocBuffer(len);
			memcpy(m_pBuffer, m_pPos, m_bytesRemaining);
			m_bufferBytes = m_bytesRemaining;
			m_bytesRemaining = 0;
			return nullptr;
		}
	}
}

