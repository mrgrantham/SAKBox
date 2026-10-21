// sha2.h: Brian Gladman's SHA-256/384/512, which WCrypto and WDynamicPage
// use. Expected digests are the standard ones (as shasum gives).
#include <gtest/gtest.h>

#include <WClasses/sha2.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include <sys/mman.h>
#include <unistd.h>

namespace {
std::string hex(const unsigned char *bytes, size_t n) {
  std::string s;
  char buf[3];
  for (size_t i = 0; i < n; i++) {
    snprintf(buf, sizeof(buf), "%02x", bytes[i]);
    s += buf;
  }
  return s;
}

// Hashes text with sha2_hash, given in pieces of 1, 2, 3, ... bytes so the
// partial-block and whole-block paths are both used
std::string sha(unsigned int bits, const std::string &text) {
  sha2_ctx ctx;
  EXPECT_EQ(sha2_begin(&ctx, bits), SHA2_GOOD);
  const auto *data = reinterpret_cast<const unsigned char *>(text.data());
  size_t done = 0;
  for (size_t piece = 1; done < text.size(); piece++) {
    const size_t n = std::min(piece, text.size() - done);
    sha2_hash(data + done, n, &ctx);
    done += n;
  }
  unsigned char digest[SHA512_DIGEST_LENGTH];
  sha2_end(digest, &ctx);
  return hex(digest, bits / 8);
}
} // namespace

TEST(Sha2Test, StandardDigests) {
  const std::string millionAs(1000000, 'a');
  EXPECT_EQ(sha(256, ""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(sha(256, "abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(sha(256, millionAs),
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
  EXPECT_EQ(sha(384, "abc"), "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
                             "8086072ba1e7cc2358baeca134c825a7");
  EXPECT_EQ(sha(384, millionAs), "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b"
                                 "07b8b3dc38ecc4ebae97ddd87f3d8985");
  EXPECT_EQ(sha(512, "abc"), "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
                             "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
  EXPECT_EQ(sha(512, millionAs),
            "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
            "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b");
}

// The byte count is two words; when the low word wraps, one is carried into
// the high word. (Reaching that by hashing would take 4 GiB for SHA-256, so
// the count is set just below the wrap.)
TEST(Sha2Test, ByteCountCarriesIntoHighWord) {
  const unsigned char block[64] = {};
  sha256_ctx c256;
  sha256_begin(&c256);
  c256.count[0] = 0xFFFFFFC0u; // a whole number of blocks, 64 bytes short of wrapping
  sha256_hash(block, 64, &c256);
  EXPECT_EQ(c256.count[0], 0u);
  EXPECT_EQ(c256.count[1], 1u);

  const unsigned char block512[128] = {};
  sha512_ctx c512;
  sha512_begin(&c512);
  c512.count[0] = 0xFFFFFFFFFFFFFF80ull;
  sha512_hash(block512, 128, &c512);
  EXPECT_EQ(c512.count[0], 0u);
  EXPECT_EQ(c512.count[1], 1u);
}

// SHA-512 (and SHA-384, which shares its code) put the message's length in
// bits into the last block as two 64-bit words, but took the high word's
// carry from bit 29 of the byte count, as SHA-256 does with 32-bit words.
// So from 512 MiB on, the length was wrong, and so was the hash. Hashing
// that much here would be slow, so the test sets the byte count directly:
// 2^32 bytes and 2^32 + 2^64 bytes came out as the same length.
TEST(Sha2Test, Sha512LengthUsesTheWholeByteCount) {
  auto digestWithCount = [](uint64_t low, uint64_t high) {
    sha512_ctx ctx;
    sha512_begin(&ctx);
    ctx.count[0] = low; // (a whole number of blocks, so nothing is buffered)
    ctx.count[1] = high;
    unsigned char digest[SHA512_DIGEST_LENGTH];
    sha512_end(digest, &ctx);
    return hex(digest, sizeof(digest));
  };
  EXPECT_NE(digestWithCount(1ull << 32, 0), digestWithCount(1ull << 32, 1));
}

// The hash functions take an unsigned long length but kept it in a
// uint32_t, so 4 GiB + 3 bytes hashed only 3. Testing that with real data
// would need a 4 GiB buffer, so this gives them one readable page followed
// by an unreadable one, and a length of 4 GiB + 3: the old code read 3
// bytes and returned; now they read on into the unreadable page and crash,
// which shows they take the whole length. (Checked once by hand too: a
// 4 GiB + 3 byte zero buffer hashed in one call matches shasum.)
TEST(Sha2DeathTest, TakesTheWholeLengthPast4GiB) {
  if (sizeof(unsigned long) < 8)
    GTEST_SKIP() << "unsigned long can't hold 4 GiB here";
  const size_t page = getpagesize();
  void *mem = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  ASSERT_NE(mem, MAP_FAILED);
  ASSERT_EQ(mprotect(static_cast<char *>(mem) + page, page, PROT_NONE), 0);
  const auto *data = static_cast<const unsigned char *>(mem);
  const unsigned long len = (1ul << 32) + 3;
  EXPECT_DEATH(
      {
        sha256_ctx ctx;
        sha256_begin(&ctx);
        sha256_hash(data, len, &ctx);
      },
      "");
  EXPECT_DEATH(
      {
        sha512_ctx ctx;
        sha512_begin(&ctx);
        sha512_hash(data, len, &ctx);
      },
      "");
  munmap(mem, 2 * page);
}
