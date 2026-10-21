/*
  The contents of this file are dedicated by all of its authors, including

    Michael S. Gashler,
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

#ifndef __GKEYPAIR_H__
#define __GKEYPAIR_H__

namespace WClasses
{

class WBigInt;
class WRandCrypto;
class WDomNode;
class WDom;

// This is my home-made (so don't trust it) implementation of symmetric key cryptography.
class WKeyPair
{
private:
	WBigInt* m_pPublicKey;
	WBigInt* m_pPrivateKey;
	WBigInt* m_pN;

public:
	WKeyPair();
	WKeyPair(WDomNode* pNode);
	virtual ~WKeyPair();

	static void test();

	// Serialize the key pair
	WDomNode* serialize(WDom* pDoc, bool bIncludePrivateKey);

	// Generates a key-pair using the 3 buffers of cryptographic uint values you pass in.
	// (Typically, you will generate the values in these buffers by using a one-way hash
	// algorithm to digest a larger buffer of entropy values collected from mouse-wiggles,
	// key-strokes, and other such sources of unpredictable values.)
	// uintCount specifies the number of cryptographically random unsigned integers in each of the 3 buffers.
	void generateKeyPair(unsigned int uintCount, const unsigned int* pRawCryptographicBytes1, const unsigned int* pRawCryptographicBytes2, const unsigned int* pRawCryptographicBytes3);

	// Takes ownership of the WBigInt you pass in
	void setPublicKey(WBigInt* pPublicKey);

	// Takes ownership of the WBigInt you pass in
	void setPrivateKey(WBigInt* pPrivateKey);

	// Takes ownership of the WBigInt you pass in
	void setN(WBigInt* pN);

	// Copies the WBigInt you pass in
	void copyPublicKey(WBigInt* pPublicKey);

	// Copies the WBigInt you pass in
	void copyPrivateKey(WBigInt* pPrivateKey);

	// Copies the WBigInt you pass in
	void copyN(WBigInt* pN);

	// Get the public part of the key (not including N which is public too)
	WBigInt* publicKey();

	// Get the private part of the key
	WBigInt* privateKey();

	// Get the N part of the key
	WBigInt* n();

	// Returns the maximum number of bytes that you can encrypt using the PowerMod method
	int maxBlockSize();

	// This is the method that encrypts/decrypts your message.
	// If bPublicKey is true, it uses the public key.  If false it uses the private key.
	// Note: you must delete the buffer this returns
	unsigned char* powerMod(const unsigned char* pInput, int nInputSize, bool bPublicKey, int* pnOutputSize);
};

} // namespace WClasses

#endif // __GKEYPAIR_H__
