// Seed.hpp - the number that picks one course, board or run, and the named
// streams a game draws from.
//
// A seed is written as a short code to show, type and share: K7QM-3XPA.
// The code is 8 characters of Crockford's base 32 (digits and capitals
// without I, L, O and U), so a seed has 40 bits: about a trillion courses.
// Typing it back is forgiving: lowercase is fine, the dash and spaces are
// optional, O reads as 0, and I and L as 1.
//
// Each part of a game draws from its own stream, named for what it's for, so
// adding a feature that draws numbers doesn't change every other sequence:
//
//   SeededRandom::Seed seed = *SeededRandom::Seed::fromCode("K7QM-3XPA");
//   SeededRandom::Generator course = seed.stream("course");
//   SeededRandom::Generator bots = seed.stream("bots");
//
// A new seed for a new game: Seed::mixedFrom(anything that varies), such as
// the time or the frame the player pressed start on.
#pragma once

#include <SeededRandom/Generator.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace SeededRandom {

class Seed {
public:
  static constexpr int kBits = 40;
  static constexpr int kCodeLength = 8; // characters, not counting the dash
  static constexpr std::uint64_t kMask = (std::uint64_t{1} << kBits) - 1;

  // A seed from its number (only the low 40 bits are kept).
  constexpr explicit Seed(std::uint64_t value) : value_(value & kMask) {}

  // A seed spread from any number, so values that differ only a little (the
  // time a second apart) give unrelated seeds.
  static constexpr Seed mixedFrom(std::uint64_t anything) {
    std::uint64_t mixer = anything;
    return Seed(splitMix64(mixer));
  }

  constexpr std::uint64_t value() const { return value_; }

  // The generator for one named part of the game. The same seed and name
  // always give the same sequence; different names give unrelated ones.
  constexpr Generator stream(std::string_view name) const {
    return Generator(value_, streamKey(name));
  }

  // The code to show: 8 characters as two groups of 4, "K7QM-3XPA".
  std::string code() const {
    std::string text(kCodeLength + 1, '-');
    for (int i = 0; i < kCodeLength; ++i) {
      const auto symbol = static_cast<int>((value_ >> (5 * (kCodeLength - 1 - i))) & 31);
      text[i < 4 ? i : i + 1] = kAlphabet[symbol];
    }
    return text;
  }

  // The seed a typed code stands for, or nothing if it isn't one (the wrong
  // length, or a character that isn't in the code).
  static constexpr std::optional<Seed> fromCode(std::string_view text) {
    std::uint64_t value = 0;
    int symbols = 0;
    for (char c : text) {
      if (c == '-' || c == ' ')
        continue;
      const int symbol = symbolOf(c);
      if (symbol < 0 || ++symbols > kCodeLength)
        return std::nullopt;
      value = (value << 5) | static_cast<std::uint64_t>(symbol);
    }
    if (symbols != kCodeLength)
      return std::nullopt;
    return Seed(value);
  }

  constexpr bool operator==(const Seed &) const = default;

private:
  static constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

  static constexpr int symbolOf(char c) {
    if (c >= 'a' && c <= 'z')
      c = static_cast<char>(c - 'a' + 'A');
    if (c == 'O')
      c = '0';
    if (c == 'I' || c == 'L')
      c = '1';
    for (int i = 0; i < 32; ++i) {
      if (kAlphabet[i] == c)
        return i;
    }
    return -1;
  }

  // A name's key: its FNV-1a hash, spread by splitmix64.
  static constexpr std::uint64_t streamKey(std::string_view name) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (char c : name) {
      hash ^= static_cast<unsigned char>(c);
      hash *= 0x100000001b3ull;
    }
    return splitMix64(hash);
  }

  std::uint64_t value_;
};

} // namespace SeededRandom
