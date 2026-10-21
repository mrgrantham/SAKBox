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

#include "WBitTable.h"
#include "WError.h"
#include <string.h> // (for memset)
#include "WRand.h"

using namespace WClasses;

#define BLOCK_BITS (sizeof(size_t) * 8)


WBitTable::WBitTable(size_t bitCount)
: m_size((bitCount + BLOCK_BITS - 1) / BLOCK_BITS),
  m_pBits(new size_t[m_size])
{
	memset(m_pBits, '\0', sizeof(size_t) * m_size);
}

WBitTable::WBitTable(const WBitTable& o)
: m_size(o.m_size), m_pBits(new size_t[m_size])
{
	memcpy(m_pBits, o.m_pBits, sizeof(size_t) * m_size);
}

WBitTable& WBitTable::operator=(const WBitTable& o)
{
	// SAKBox: t = t freed the bits and then copied from the freed memory
	if(this == &o)
		return *this;
	delete[] m_pBits;
	m_size = o.m_size;
	m_pBits = new size_t[m_size];
	memcpy(m_pBits, o.m_pBits, sizeof(size_t) * m_size);
	return *this;
}


WBitTable::~WBitTable()
{
	delete[] m_pBits;
}

void WBitTable::clearAll()
{
	memset(m_pBits, '\0', sizeof(size_t) * m_size);
}

void WBitTable::setAll()
{
	memset(m_pBits, 255, sizeof(size_t) * m_size);
}

bool WBitTable::bit(size_t index) const
{
	WAssert(index < m_size * BLOCK_BITS); // out of range
	size_t n = m_pBits[index / BLOCK_BITS];
	size_t m = index & (BLOCK_BITS - 1);
	return ((n & ((size_t)1 << m)) ? true : false);
}

void WBitTable::set(size_t index)
{
	WAssert(index < m_size * BLOCK_BITS); // out of range
	size_t m = index & (BLOCK_BITS - 1);
	m_pBits[index / BLOCK_BITS] |= ((size_t)1 << m);
}

void WBitTable::unset(size_t index)
{
	WAssert(index < m_size * BLOCK_BITS); // out of range
	size_t m = index & (BLOCK_BITS - 1);
	m_pBits[index / BLOCK_BITS] &= (~((size_t)1 << m));
}

void WBitTable::toggle(size_t index)
{
	WAssert(index < m_size * BLOCK_BITS); // out of range
	size_t m = index & (BLOCK_BITS - 1);
	m_pBits[index / BLOCK_BITS] ^= ((size_t)1 << m);
}

bool WBitTable::equals(const WBitTable& that) const
{
	if(this->m_size != that.m_size)
		return false;
	for(size_t i = 0; i < m_size; i++)
	{
		if(this->m_pBits[i] != that.m_pBits[i])
			return false;
	}
	return true;
}

bool WBitTable::areAllSet(size_t count)
{
	size_t head = count / BLOCK_BITS;
	size_t tail = count % BLOCK_BITS;
	size_t* pBits = m_pBits;
	for(size_t i = 0; i < head; i++)
	{
		if(*(pBits++) != ~((size_t)0))
			return false;
	}
	if(tail > 0)
	{
		if((*pBits | ~((((size_t)1) << tail) - 1)) != ~((size_t)0))
			return false;
	}
	return true;
}

bool WBitTable::areAllClear(size_t count)
{
	size_t head = count / BLOCK_BITS;
	size_t tail = count % BLOCK_BITS;
	size_t* pBits = m_pBits;
	for(size_t i = 0; i < head; i++)
	{
		if(*(pBits++) != (size_t)0)
			return false;
	}
	if(tail > 0)
	{
		if((*pBits & ((((size_t)1) << tail) - 1)) != (size_t)0)
			return false;
	}
	return true;
}

#define TEST_SIZE 2080
// static
void WBitTable::test()
{
	WRand prng(0);
	bool arr[TEST_SIZE];
	for(size_t i = 0; i < TEST_SIZE; i++)
		arr[i] = false;
	WBitTable bt(TEST_SIZE);
	for(size_t i = 0; i < TEST_SIZE; i++)
	{
		size_t n = (size_t)prng.next(TEST_SIZE);
		bt.toggle(n);
		arr[n] = !arr[n];
		n = (size_t)prng.next(TEST_SIZE);
		bool b = (prng.next(2) == 0 ? false : true);
		arr[n] = b;
		if(b)
			bt.set(n);
		else
			bt.unset(n);
	}
	for(size_t i = 0; i < TEST_SIZE; i++)
	{
		if(bt.bit(i) != arr[i])
			throw Ex("failed");
	}

	WBitTable bt2(81);
	if(!bt2.areAllClear(81))
		throw Ex("failed");
	bt2.set(80);
	if(bt2.areAllClear(81))
		throw Ex("failed");
	if(!bt2.areAllClear(80))
		throw Ex("failed");
	for(size_t i = 0; i < 79; i++)
		bt2.set(i);
	if(bt2.areAllSet(81))
		throw Ex("failed");
	bt2.set(79);
	if(!bt2.areAllSet(81))
		throw Ex("failed");
}
