#include <CosmicHex/Handheld/PlayerNames.hpp>

#include <SeededRandom/Generator.hpp>

#include <algorithm>
#include <cctype>
#include <vector>

namespace CosmicHex::Handheld {

namespace {
// Short, easy to tell apart at a glance, each fitting a tab
constexpr std::array<std::string_view, 12> kStars{"Vega",   "Orion", "Lyra",   "Rigel",
                                                  "Nova",   "Atlas", "Juno",   "Cygnus",
                                                  "Altair", "Mira",  "Sirius", "Draco"};

bool sameName(std::string_view a, std::string_view b) {
  return std::ranges::equal(a, b, [](char x, char y) {
    return std::tolower(static_cast<unsigned char>(x)) ==
           std::tolower(static_cast<unsigned char>(y));
  });
}
} // namespace

PlayerNames PlayerNames::forGame(int players, int person, std::string_view yours,
                                 std::uint64_t seed) {
  PlayerNames out;
  std::vector<std::string_view> stars;
  for (const std::string_view star : kStars)
    if (!sameName(star, yours))
      stars.push_back(star);
  // (a shuffle by the game's seed: a different few each game)
  SeededRandom::Generator random(seed);
  for (std::size_t i = stars.size(); i > 1; --i)
    std::swap(stars[i - 1], stars[random.below(static_cast<std::uint32_t>(i))]);
  std::size_t next = 0;
  for (int p = 0; p < players && p < kMaxPlayers; ++p)
    out.names_[static_cast<std::size_t>(p)] =
        p == person ? (yours.empty() ? std::string("You") : std::string(yours))
                    : std::string(stars[next++]);
  return out;
}

const std::string &PlayerNames::operator[](int player) const {
  static const std::string kNobody;
  return player >= 0 && player < kMaxPlayers ? names_[static_cast<std::size_t>(player)] : kNobody;
}

std::string PlayerNames::shortOf(int player) const {
  return (*this)[player].substr(0, kShortLetters);
}

std::string PlayerNames::upperOf(int player) const {
  std::string upper = (*this)[player];
  std::ranges::transform(upper, upper.begin(),
                         [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return upper;
}

} // namespace CosmicHex::Handheld
