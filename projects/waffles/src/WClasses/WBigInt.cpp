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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "WBigInt.h"
#include "WError.h"
#include "WHolders.h"
#include "WKeyPair.h"
#include "WDom.h"
#ifndef WINDOWS
#include <sys/types.h>
typedef int64_t __int64;
#endif // not WINDOWS

namespace WClasses
{

WBigInt::WBigInt()
{
	m_pBits = nullptr;
	m_nUInts = 0;
	m_bSign = true;
}

WBigInt::WBigInt(WDomNode* pNode)
{
	WDomListIterator it(pNode);
	m_nUInts = (unsigned int)it.remaining();
	m_pBits = new unsigned int[m_nUInts - 1];
	if(it.currentInt() >= 0)
		m_bSign = true;
	else
		m_bSign = false;
	it.advance();
	for(unsigned int i = 0; i < m_nUInts; i++)
	{
		m_pBits[i] = (unsigned int)it.currentInt();
		it.advance();
	}
}

WBigInt::~WBigInt()
{
	delete[] m_pBits;
}

WDomNode* WBigInt::serialize(WDom* pDoc) const
{
	WDomNode* pNode = pDoc->newList();
	pNode->add(pDoc, m_bSign ? 1ll : -1ll);
	for(unsigned int i = 0; i < m_nUInts; i++)
		pNode->add(pDoc, (long long)m_pBits[i]);
	return pNode;
}

unsigned int WBigInt::getBitCount()
{
	if(m_nUInts < 1)
		return 0;
	unsigned int nEnd = m_nUInts - 1;
	unsigned int nBitCount = m_nUInts * BITS_PER_INT;
	while(m_pBits[nEnd] == 0)
	{
		nBitCount -= BITS_PER_INT;
		if(nEnd == 0)
			return 0;
		nEnd--;
	}
	unsigned int nVal = m_pBits[nEnd];
	unsigned int nMask = 1 << (BITS_PER_INT - 1);
	while((nVal & nMask) == 0)
	{
		nBitCount--;
		nMask >>= 1;
	}
	return nBitCount;
}

void WBigInt::resize(unsigned int nBits)
{
	if(nBits < 1)
	{
		delete[] m_pBits;
		m_pBits = nullptr;
		m_nUInts = 0;
		return;
	}
	unsigned int nNewUInts = ((nBits - 1) / BITS_PER_INT) + 1;
	if(nNewUInts <= m_nUInts && nNewUInts + 2 > m_nUInts / 2) // magic heuristic
	{
		unsigned int i;
		for(i = nNewUInts; i < m_nUInts; i++)
			m_pBits[i] = 0;
		return;
	}
	unsigned int* pNewBits = new unsigned int[nNewUInts];
	unsigned int nTop = m_nUInts;
	if(nNewUInts < nTop)
		nTop = nNewUInts;
	unsigned int n;
	for(n = 0; n < nTop; n++)
		pNewBits[n] = m_pBits[n];
	for(; n < nNewUInts; n++)
		pNewBits[n] = 0;
	delete[] m_pBits;
	m_pBits = pNewBits;
	m_nUInts = nNewUInts;
}

void WBigInt::setBit(unsigned int nPos, bool bVal)
{
	if(nPos >= m_nUInts * BITS_PER_INT)
		resize(nPos + 1);
	if(bVal)
		m_pBits[nPos / BITS_PER_INT] |= (1 << (nPos % BITS_PER_INT));
	else
		m_pBits[nPos / BITS_PER_INT] &= ~(1 << (nPos % BITS_PER_INT));
}

void WBigInt::setUInt(unsigned int nPos, unsigned int nVal)
{
	if(nPos >= m_nUInts)
		resize((nPos + 1) * BITS_PER_INT);
	m_pBits[nPos] = nVal;
}

void WBigInt::copy(WBigInt* pBigNumber)
{
	if(m_nUInts < pBigNumber->m_nUInts)
		resize(pBigNumber->m_nUInts * BITS_PER_INT);
	if(m_nUInts < 1)
		return;
	unsigned int n;
	for(n = 0; n < pBigNumber->m_nUInts; n++)
		m_pBits[n] = pBigNumber->m_pBits[n];
	for(; n < m_nUInts; n++)
		m_pBits[n] = 0;
	m_bSign = pBigNumber->m_bSign;
}

void WBigInt::setToZero()
{
	memset(m_pBits, '\0', m_nUInts * sizeof(unsigned int));
	m_bSign = true;
}

bool WBigInt::isZero()
{
	if(m_nUInts < 1)
		return true;
	unsigned int n;
	for(n = 0; n < m_nUInts; n++)
	{
		if(m_pBits[n] != 0)
			return false;
	}
	return true;
}

unsigned int* WBigInt::toBufferGiveOwnership()
{
	unsigned int* pBuffer = m_pBits;
	m_bSign = true;
	m_nUInts = 0;
	m_pBits = nullptr;
	return pBuffer;
}

bool WBigInt::toBuffer(unsigned int* pBuffer, int nBufferSize)
{
	int nSize = getUIntCount();
	if(nBufferSize < nSize)
		return false;
	int n;
	for(n = nSize - 1; n >= 0; n--)
		pBuffer[n] = m_pBits[n];
	return true;
}

void WBigInt::fromBuffer(const unsigned int* pBuffer, int nBufferSize)
{
	setToZero();
	int n;
	for(n = nBufferSize - 1; n >= 0; n--)
		setUInt(n, pBuffer[n]);
}

void WBigInt::fromByteBuffer(const unsigned char* pBuffer, int nBufferChars)
{
	// Make sure input is alligned to sizeof(unsigned int)
	int nTempBufSize = nBufferChars + sizeof(unsigned int) - 1;
	WTEMPBUF(unsigned char, pBuf, nTempBufSize);
	if((nBufferChars % sizeof(unsigned int)) != 0)
	{
		memset(pBuf, '\0', nBufferChars + sizeof(unsigned int) - 1);
		memcpy(pBuf, pBuffer, nBufferChars);
		pBuffer = pBuf;
		nBufferChars += sizeof(unsigned int) - 1;
	}
	nBufferChars /= sizeof(unsigned int);
	fromBuffer((unsigned int*)pBuffer, nBufferChars);
}

bool WBigInt::toHex(char* szBuff, int nBufferSize)
{
	bool bStarted = false;
	int nUInts = getUIntCount();
	int n, i;
	unsigned char byte;
	char c;
	int nPos = 0;
	for(n = nUInts - 1; n >= 0; n--)
	{
		for(i = sizeof(int) * 2 - 1; i >= 0; i--)
		{
			byte = (m_pBits[n] >> (4 * i)) & 15;
			if(byte == 0 && !bStarted)
				continue;
			bStarted = true;
			c = byte < 10 ? '0' + byte : 'a' - 10 + byte;
			szBuff[nPos] = c;
			nPos++;
			if(nPos >= nBufferSize)
				return false;
		}
	}
	szBuff[nPos] = '\0';
	return true;
}

bool WBigInt::fromHex(const char* szHexValue)
{
	unsigned int nLength = (unsigned int)strlen(szHexValue);
	resize(nLength * 4);
	setToZero();
	unsigned int nUIntPos = 0;
	unsigned int nHexCount = 0;
	unsigned int n;
	for(n = 0; n < nLength; n++)
	{
		unsigned int nTmp;
		char cTmp = szHexValue[nLength - n - 1];
		if(cTmp >= '0' && cTmp <= '9')
			nTmp = cTmp - '0';
		else if(cTmp >= 'A' && cTmp <= 'F')
			nTmp = cTmp - 'A' + 10;
		else if(cTmp >= 'a' && cTmp <= 'f')
			nTmp = cTmp - 'a' + 10;
		else
			return false;
		m_pBits[nUIntPos] |= (nTmp << (4 * nHexCount));
		nHexCount++;
		if(nHexCount >= sizeof(unsigned int) * 2)
		{
			nHexCount = 0;
			nUIntPos++;
		}
	}
	return true;
}

void WBigInt::negate()
{
	m_bSign = !m_bSign;
}

int WBigInt::compareTo(WBigInt* pOperand)
{
	if(m_bSign != pOperand->m_bSign)
	{
		if(isZero() && pOperand->isZero())
			return 0;
		return m_bSign ? 1 : -1;
	}
	int nCmp = 0;
	unsigned int nA;
	unsigned int nB;
	unsigned int n = m_nUInts;
	if(pOperand->m_nUInts > n)
		n = pOperand->m_nUInts;
	n--;
	while(true)
	{
		nA = getUInt(n);
		nB = pOperand->getUInt(n);
		if(nA != nB)
		{
			nCmp = nA > nB ? 1 : -1;
			break;
		}
		if(n == 0)
			break;
		n--;
	}
	return m_bSign ? nCmp : -nCmp;
}

void WBigInt::increment()
{
	if(!m_bSign)
	{
		negate();
		decrement();
		negate();
		return;
	}
	unsigned int n;
	for(n = 0; true; n++)
	{
		if(n == m_nUInts)
			resize(m_nUInts * BITS_PER_INT + 1);
		m_pBits[n]++;
		if(m_pBits[n] != 0)
			return;
	}
}

void WBigInt::decrement()
{
	if(!m_bSign)
	{
		negate();
		increment();
		negate();
		return;
	}
	if(isZero())
	{
		increment();
		negate();
		return;
	}
	unsigned int n;
	for(n = 0; true; n++)
	{
		if(m_pBits[n] == 0)
			m_pBits[n]--;
		else
		{
			m_pBits[n]--;
			return;
		}
	}
}

void WBigInt::add(WBigInt* pBigNumber)
{
	// Check signs
	if(!m_bSign)
	{
		negate();
		subtract(pBigNumber);
		negate();
		return;
	}
	if(!pBigNumber->m_bSign)
	{
		pBigNumber->negate();
		subtract(pBigNumber);
		pBigNumber->negate();
		return;
	}

	// See if we need a bigger buffer
	unsigned int nBits = getBitCount();
	unsigned int nTmp = pBigNumber->getBitCount();
	if(nTmp > nBits)
		nBits = nTmp;
	nBits++;
	if(nBits > m_nUInts * BITS_PER_INT)
		resize(nBits);

	// Add it up
	unsigned int nSum;
	unsigned int n;
	unsigned int nOperand;
	bool bNextCarry;
	bool bCarry = false;
	for(n = 0; n < m_nUInts; n++)
	{
		nOperand = pBigNumber->getUInt(n);
		nSum = m_pBits[n] + nOperand;
		if(nSum < m_pBits[n] && nSum < nOperand)
			bNextCarry = true;
		else
			bNextCarry = false;
		if(bCarry)
		{
			if(++nSum == 0)
				bNextCarry = true;
		}
		bCarry = bNextCarry;
		m_pBits[n] = nSum;
	}
}

void WBigInt::subtract(WBigInt* pBigNumber)
{
	// Check signs
	if(!m_bSign)
	{
		negate();
		add(pBigNumber);
		negate();
		return;
	}
	if(!pBigNumber->m_bSign)
	{
		pBigNumber->negate();
		add(pBigNumber);
		pBigNumber->negate();
		return;
	}

	// Check sizes
	WBigInt tmp;
	WBigInt* pA = this;
	WBigInt* pB = pBigNumber;
	int nCmp = compareTo(pBigNumber);
	if(nCmp < 0)
	{
		tmp.copy(pBigNumber);
		pA = &tmp;
		pB = this;
	}

	// Subtract
	unsigned int n;
	unsigned int nA;
	unsigned int nB;
	bool bNextBorrow;
	bool bBorrow = false;
	for(n = 0; n < pA->m_nUInts; n++)
	{
		nA = pA->m_pBits[n];
		nB = pB->getUInt(n);
		bNextBorrow = false;
		if(bBorrow)
		{
			if(nA == 0)
				bNextBorrow = true;
			nA--;
		}
		if(nB > nA)
			bNextBorrow = true;
		pA->m_pBits[n] = nA - nB;
		bBorrow = bNextBorrow;
	}

	// Negate again if we swapped A and B
	if(nCmp < 0)
	{
		tmp.negate();
		copy(&tmp);
	}
}

void WBigInt::shiftLeft(unsigned int nBits)
{
	shiftLeftBits(nBits % BITS_PER_INT);
	shiftLeftUInts(nBits / BITS_PER_INT);
}

void WBigInt::shiftLeftBits(unsigned int nBits)
{
	if(m_nUInts == 0)
		return;
	if(nBits == 0)
		return;
	if(m_pBits[m_nUInts - 1] != 0)
		resize(getBitCount() + nBits);
	unsigned int n;
	unsigned int nCarry = 0;
	unsigned int nNextCarry;
	for(n = 0; n < m_nUInts; n++)
	{
		nNextCarry = m_pBits[n] >> (BITS_PER_INT - nBits);
		m_pBits[n] <<= nBits;
		m_pBits[n] |= nCarry;
		nCarry = nNextCarry;
	}
}

void WBigInt::shiftLeftUInts(unsigned int nUInts)
{
	if(m_nUInts == 0)
		return;
	if(nUInts == 0)
		return;
	if(!(nUInts == 1 && m_pBits[m_nUInts - 1] == 0)) // optimization to make Multiply faster
		resize((m_nUInts + nUInts) * BITS_PER_INT);
	unsigned int n = m_nUInts - 1;
	if(n >= nUInts)
	{
		while(true)
		{
			m_pBits[n] = m_pBits[n - nUInts];
			if(n - nUInts == 0)
			{
				n--;
				break;
			}
			n--;
		}
	}
	while(true)
	{
		m_pBits[n] = 0;
		if(n == 0)
			break;
		n--;
	}
	return;
}

void WBigInt::shiftRight(unsigned int nBits)
{
	shiftRightBits(nBits % BITS_PER_INT);
	shiftRightUInts(nBits / BITS_PER_INT);
}

void WBigInt::shiftRightBits(unsigned int nBits)
{
	if(m_nUInts == 0)
		return;
	if(nBits == 0)
		return;
	unsigned int n = m_nUInts - 1;
	unsigned int nCarry = 0;
	unsigned int nNextCarry;
	while(true)
	{
		nNextCarry = m_pBits[n] << (BITS_PER_INT - nBits);
		m_pBits[n] >>= nBits;
		m_pBits[n] |= nCarry;
		nCarry = nNextCarry;
		if(n == 0)
			break;
		n--;
	}
}

void WBigInt::shiftRightUInts(unsigned int nUInts)
{
	if(m_nUInts == 0)
		return;
	if(nUInts == 0)
		return;
	unsigned int n;
	for(n = 0; n + nUInts < m_nUInts; n++)
		m_pBits[n] = m_pBits[n + nUInts];
	for(; n < m_nUInts; n++)
		m_pBits[n] = 0;
}

void WBigInt::Or(WBigInt* pBigNumber)
{
	if(m_nUInts < pBigNumber->m_nUInts)
		resize(pBigNumber->m_nUInts * BITS_PER_INT);
	unsigned int n;
	for(n = 0; n < m_nUInts; n++)
		m_pBits[n] |= pBigNumber->getUInt(n);
}

void WBigInt::And(WBigInt* pBigNumber)
{
	unsigned int n;
	for(n = 0; n < m_nUInts; n++)
		m_pBits[n] &= pBigNumber->getUInt(n);
}

void WBigInt::Xor(WBigInt* pBigNumber)
{
	if(m_nUInts < pBigNumber->m_nUInts)
		resize(pBigNumber->m_nUInts * BITS_PER_INT);
	unsigned int n;
	for(n = 0; n < m_nUInts; n++)
		m_pBits[n] ^= pBigNumber->getUInt(n);
}

void WBigInt::multiply(WBigInt* pBigNumber, unsigned int nUInt)
{
	if(nUInt == 0)
	{
		setToZero();
		return;
	}
	setToZero();
	resize((pBigNumber->m_nUInts + 1) * BITS_PER_INT);
	unsigned int n;
	for(n = 0; n < pBigNumber->m_nUInts; n++)
	{
#ifdef BYTE_ORDER_BIG_ENDIAN
		__int64 prod = (__int64)pBigNumber->m_pBits[n] * (__int64)nUInt;
		__int64 rev;
		((unsigned int*)&rev)[0] = ((unsigned int*)&prod)[1];
		((unsigned int*)&rev)[1] = ((unsigned int*)&prod)[0];
		*((__int64*)&m_pBits[n]) += rev;
#else // BYTE_ORDER_BIG_ENDIAN
		*((__int64*)&m_pBits[n]) += (__int64)pBigNumber->m_pBits[n] * (__int64)nUInt;
#endif // !BYTE_ORDER_BIG_ENDIAN
	}
}

void WBigInt::multiply(WBigInt* pFirst, WBigInt* pSecond)
{
	setToZero();
	resize(pFirst->getBitCount() + pSecond->getBitCount());
	WBigInt tmp;
	unsigned int nUInts = pFirst->m_nUInts;
	if(pSecond->m_nUInts > nUInts)
		nUInts = pSecond->m_nUInts;
	unsigned int n;
	for(n = 0; n < nUInts; n++)
	{
		shiftLeftUInts(1);
		tmp.multiply(pFirst, pSecond->getUInt(nUInts - 1 - n));
		add(&tmp);
	}
	m_bSign = ((pFirst->m_bSign == pSecond->m_bSign) ? true : false);
}

void WBigInt::divide(WBigInt* pInNominator, WBigInt* pInDenominator, WBigInt* pOutRemainder)
{
	setToZero();
	resize(pInNominator->getBitCount());
	pOutRemainder->setToZero();
	pOutRemainder->resize(pInDenominator->getBitCount());
	unsigned int nBits = pInNominator->getBitCount();
	unsigned int n;
	for(n = 0; n < nBits; n++)
	{
		pOutRemainder->shiftLeftBits(1);
		pOutRemainder->setBit(0, pInNominator->getBit(nBits - 1 - n));
		shiftLeftBits(1);
		if(pOutRemainder->compareTo(pInDenominator) >= 0)
		{
			setBit(0, true);
			pOutRemainder->subtract(pInDenominator);
		}
		else
		{
			setBit(0, false);
		}
	}
	m_bSign = ((pInNominator->m_bSign == pInDenominator->m_bSign) ? true : false);
	pOutRemainder->m_bSign = pInNominator->m_bSign;
}

// DO NOT use for crypto
// This is NOT a cryptographic random number generator
void WBigInt::setRandom(unsigned int nBits)
{
	resize(nBits);
	unsigned int nBytes = nBits / 8;
	unsigned int nExtraBits = nBits % 8;
	unsigned int n;
	for(n = 0; n < nBytes; n++)
		((unsigned char*)m_pBits)[n] = (unsigned char)rand();
	if(nExtraBits > 0)
	{
		unsigned char c = (unsigned char)rand();
		c <<= (8 - nExtraBits);
		c >>= (8 - nExtraBits);
		((unsigned char*)m_pBits)[n] = c;
	}
}

// Input:  integers a, b
// Output: [this,x,y] where "this" is the greatest common divisor of a,b and where g=xa+by (x or y can be negative)
void WBigInt::euclid(WBigInt* pA1, WBigInt* pB1, WBigInt* pOutX /*=NULL*/, WBigInt* pOutY /*=NULL*/)
{
	WBigInt q;
	WBigInt r;
	WBigInt x;
	WBigInt y;

	WBigInt a;
	a.copy(pA1);
	a.setSign(true);

	WBigInt b;
	b.copy(pB1);
	b.setSign(true);

	WBigInt x0;
	x0.increment();
	x0.setSign(pA1->getSign());
	WBigInt x1;
	WBigInt y0;
	WBigInt y1;
	y1.increment();
	y1.setSign(pB1->getSign());

	while(!b.isZero())
	{
		q.divide(&a, &b, &r);
		a.copy(&b);
		b.copy(&r);

		x.multiply(&x1, &q);
		x.negate();
		x.add(&x0);

		x0.copy(&x1);
		x1.copy(&x);

		y.multiply(&y1, &q);
		y.negate();
		y.add(&y0);

		y0.copy(&y1);
		y1.copy(&y);
	}
	copy(&a);
	if(pOutX)
		pOutX->copy(&x0);
	if(pOutY)
		pOutY->copy(&y0);
}


// Input:  pProd is the product of (p - 1) * (q - 1) where p and q are prime
//         pRandomData is some random data that will be used to pick the key.
// Output: It will return a key that has no common factors with pProd.  It starts
//         with the random data you provide and increments it until it fits this
//         criteria.
void WBigInt::selectPublicKey(const unsigned int* pRandomData, int nRandomDataUInts, WBigInt* pProd)
{
	// copy random data
	setToZero();
	int n;
	for(n = nRandomDataUInts - 1; n >= 0; n--)
		setUInt(n, pRandomData[n]);

	// increment until this number has no common factor with pProd
	WBigInt tmp;
	while(true)
	{
		tmp.euclid(this, pProd);
		tmp.decrement();
		if(tmp.isZero())
			break;
		increment();
	}
}

// (Used by CalculatePrivateKey)
void EuclidSwap(WBigInt* a, WBigInt* b, WBigInt* c)
{
	WBigInt t1;
	t1.copy(a);
	a->copy(b);
	WBigInt tmp;
	tmp.multiply(b, c);
	b->copy(&t1);
	b->subtract(&tmp);
}

// Input:  pProd is the product of (p - 1) * (q - 1) where p and q are prime
//         pPublicKey is a number that has no common factors with pProd
// Output: this will become a private key to go with the public key
void WBigInt::calculatePrivateKey(WBigInt* pPublicKey, WBigInt* pProd)
{
	WBigInt d;
	WBigInt q;
	WBigInt d2;
	WBigInt u;
	WBigInt u2;
	WBigInt v;
	WBigInt v2;
	WBigInt r; // holds a remainder (unused)

	d.copy(pPublicKey);
	d2.copy(pProd);

	u.increment();
	v2.increment();

	while(true)
	{
		if(d2.isZero())
			break;

		q.divide(&d, &d2, &r);

		EuclidSwap(&d, &d2, &q);
		EuclidSwap(&u, &u2, &q);
		EuclidSwap(&v, &v2, &q);
	}

	if(!u.getSign())
		u.add(pProd);
	copy(&u);
}

// Input:  a, k>=0, n>=2
// Output: "this" where "this" = (a^k)%n   (^ = exponent operator, not xor operatore)
void WBigInt::powerMod(WBigInt* pA, WBigInt* pK, WBigInt* pN)
{
	WBigInt k;
	k.copy(pK);
	WBigInt c;
	c.copy(pA);
	WBigInt b;
	b.increment();
	WBigInt p;
	WBigInt q;
	while(!k.isZero())
	{
		if(k.getBit(0))
		{
			k.decrement();
			p.multiply(&b, &c);
			q.divide(&p, pN, &b);
		}
		p.multiply(&c, &c);
		q.divide(&p, pN, &c);
		k.shiftRightBits(1);
	}
	copy(&b);
}

// Input:  n>=3, a where 2<=a<n
// Output: "true" if this is either prime or a strong pseudoprime to base a,
//		   "false" otherwise
bool WBigInt::millerRabin(WBigInt* pA)
{
	if(!getBit(0))
		return false;
	WBigInt g;
	g.euclid(pA, this);
	WBigInt one;
	one.increment();
	if(g.compareTo(&one) > 0)
		return false;
	WBigInt m;
	m.copy(this);
	m.decrement();
	WBigInt s;
	while(!m.getBit(0))
	{
		m.shiftRightBits(1);
		s.increment();
	}
	WBigInt b;
	b.powerMod(pA, &m, this);
	if(b.compareTo(&one) == 0)
		return true;
	decrement();
	int nCmp = b.compareTo(this);
	increment();
	if(nCmp == 0)
		return true;
	WBigInt i;
	WBigInt b1;
	i.increment();
	while(i.compareTo(&s) < 0)
	{
		m.multiply(&b, &b);
		g.divide(&m, this, &b1);
		decrement();
		nCmp = b1.compareTo(this);
		increment();
		if(nCmp == 0)
			return true;

		if(b1.compareTo(&one) == 0)
			return false;
		b.copy(&b1);
		i.increment();
	}
	return false;
}

// Output: true = pretty darn sure (99.99%) it's prime
//		   false = definately (100%) not prime
bool WBigInt::isPrime()
{
	// Two is prime.  All values less than Two are not prime
	WBigInt two;
	two.increment();
	two.increment();
	int nCmp = compareTo(&two);
	if(nCmp < 1)
	{
		if(nCmp == 0)
			return true; // two is prime
		else
			return false; // values less than two are not prime
	}

	// 9 and 15 are the only 4-bit odd primes
	unsigned int nBits = getBitCount();
	if(nBits <= 4)
	{
		unsigned int nValue = getUInt(0);
		if((nValue & 1) == 0)
			return false;
		if(nValue == 9 || nValue == 15)
			return false;
		return true;
	}

	// Do 25 iterations of Miller-Rabin
	nBits--;
	unsigned int i;
	WBigInt a;
	for(i = 1; i <= 25; i++)
	{
		a.setRandom(nBits);
		if(a.compareTo(&two) < 0)
		{
			i--;
			continue;
		}
		if(!millerRabin(&a))
			return false;
	}
	return true;
}

} // namespace WClasses

