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

#ifndef __GBIGINT_H__
#define __GBIGINT_H__

namespace WClasses
{

class WKeyPair;
class WDomNode;
class WDom;

/// Represents an integer of arbitrary size, and provides basic
/// arithmetic functionality. Also contains functionality for
/// implementing RSA symmetric-key cryptography.
class WBigInt
{
protected:
	enum
	{
		BITS_PER_INT = sizeof(unsigned int) * 8,
	};

	unsigned int m_nUInts;
	unsigned int* m_pBits;
	bool m_bSign;

public:
	WBigInt();
	WBigInt(WDomNode* pNode);
	virtual ~WBigInt();

	/// Marshal this object into a DOM that can be converted to a variety of serial formats.
	WDomNode* serialize(WDom* pDoc) const;

	/// Returns true if the number is positive and false if it is negative
	bool getSign() { return m_bSign; }

	/// Makes the number positive if bSign is true and negative if bSign is false
	void setSign(bool bSign) { m_bSign = bSign; }

	/// Returns the number of bits in the number
	unsigned int getBitCount();

	/// Returns the value of the nth bit where 0 represents the least significant bit (little endian)
	bool getBit(unsigned int n)
	{
		return ((n >= m_nUInts * BITS_PER_INT) ? false : ((m_pBits[n / BITS_PER_INT] & (1 << (n % BITS_PER_INT))) ? true : false));
	}

	/// Sets the value of the nth bit where 0 represents the least significant bit (little endian)
	void setBit(unsigned int nPos, bool bVal);

	/// Returns the number of unsigned integers required to represent this number
	unsigned int getUIntCount() { return m_nUInts; }

	/// Returns the nth unsigned integer used to represent this number
	unsigned int getUInt(unsigned int nPos) { return nPos >= m_nUInts ? 0 : m_pBits[nPos]; }

	/// Sets the value of the nth unsigned integer used to represent this number
	void setUInt(unsigned int nPos, unsigned int nVal);

	/// Sets the number to zero
	void setToZero();

	/// Returns true if the number is zero
	bool isZero();

	/// Returns -1 if this is less than pBigNumber
	/// Returns 0 if this is equal to pBigNumber
	/// Returns 1 if this is greater than pBigNumber
	int compareTo(WBigInt* pBigNumber);

	/// Copies the value of pBigNumber into this object
	void copy(WBigInt* pBigNumber);

	/// Produces a big endian hexadecimal representation of this number
	bool toHex(char* szBuff, int nBufferSize);

	/// Extract a value from a big endian hexadecimal string
	bool fromHex(const char* szHexValue);

	/// This gives you ownership of the buffer.  (You must delete it.)  It also sets the value to zero.
	unsigned int* toBufferGiveOwnership();

	/// Serializes the number. little-Endian (first bit in buffer will be LSB)
	bool toBuffer(unsigned int* pBuffer, int nBufferSize);

	/// Deserializes the number. little-Endian (first bit in buffer will be LSB)
	void fromBuffer(const unsigned int* pBuffer, int nBufferSize);

	/// Deserializes the number.
	void fromByteBuffer(const unsigned char* pBuffer, int nBufferChars);

	/// Multiplies the number by -1
	void negate();

	/// Adds one to the number
	void increment();

	/// Subtracts one from the number
	void decrement();

	/// Add another big number to this one
	void add(WBigInt* pBigNumber);

	/// Subtract another big number from this one
	void subtract(WBigInt* pBigNumber);

	/// Set this value to the product of another big number and an unsigned integer
	void multiply(WBigInt* pBigNumber, unsigned int nUInt);

	/// Set this value to the product of two big numbers
	void multiply(WBigInt* pFirst, WBigInt* pSecond);

	/// Set this value to the ratio of two big numbers and return the remainder
	void divide(WBigInt* pInNominator, WBigInt* pInDenominator, WBigInt* pOutRemainder);

	/// Shift left (multiply by 2)
	void shiftLeft(unsigned int nBits);

	/// Shift right (divide by 2 and round down)
	void shiftRight(unsigned int nBits);

	/// bitwise or
	void Or(WBigInt* pBigNumber);

	/// bitwise and
	void And(WBigInt* pBigNumber);

	/// bitwise xor
	void Xor(WBigInt* pBigNumber);

	/// Input:  integers a, b
	/// Output: this will be set to the greatest common divisor of a,b.
	///         (If pOutX and pOutY are not NULL, they will be values such
	///          that "this" = ax + by.)
	void euclid(WBigInt* pA1, WBigInt* pB1, WBigInt* pOutX = nullptr, WBigInt* pOutY = nullptr);

	/// Input:  a, k>=0, n>=2
	/// Output: this will be set to ((a raised to the power of k) modulus n)
	void powerMod(WBigInt* pA, WBigInt* pK, WBigInt* pN);

	/// Input:  "this" must be >= 3, and 2 <= a < "this"
	/// Output: "true" if this is either prime or a strong pseudoprime to base a,
	///         "false" otherwise
	bool millerRabin(WBigInt* pA);

	/// Output: true = pretty darn sure (like 99.999%) it's prime
	///         false = definately (100%) not prime
	bool isPrime();

	/// Input:  pProd is the product of (p - 1) * (q - 1) where p and q are prime
	///         pRandomData is some random data that will be used to pick the key.
	/// Output: It will return a key that has no common factors with pProd.  It starts
	///         with the random data you provide and increments it until it fits this
	///         criteria.
	void selectPublicKey(const unsigned int* pRandomData, int nRandomDataUInts, WBigInt* pProd);

	/// Input:  pProd is the product of (p - 1) * (q - 1) where p and q are prime
	///         pPublicKey is a number that has no common factors with pProd
	/// Output: this will become a private key to go with the public key
	void calculatePrivateKey(WBigInt* pPublicKey, WBigInt* pProd);

	/// DO NOT use for crypto--This is NOT a cryptographic random number generator
	void setRandom(unsigned int nBits);

protected:
	void resize(unsigned int nBits);
	void shiftLeftBits(unsigned int nBits);
	void shiftRightBits(unsigned int nBits);
	void shiftLeftUInts(unsigned int nBits);
	void shiftRightUInts(unsigned int nBits);
};

} // namespace WClasses

#endif // __GBIGINT_H__
