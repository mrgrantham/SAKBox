#include <CosmicHex/Handheld/BoardMotion.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <algorithm>
#include <numbers>

namespace CosmicHex::Handheld {

BoardMotion::BoardMotion(const State &before, const State &after) {
  // Ships match one for one when no ship came or went: a move changes one
  // ship's corner and nothing else
  const bool sameShips =
      before.ships.size() == after.ships.size() &&
      std::equal(before.ships.begin(), before.ships.end(), after.ships.begin(),
                 [](const Ship &a, const Ship &b) { return a.owner == b.owner; });
  for (std::size_t i = 0; i < after.ships.size(); ++i) {
    shipTo_.push_back(BoardPieces::at(after.ships[i].corner));
    shipFrom_.push_back(sameShips ? BoardPieces::at(before.ships[i].corner) : shipTo_.back());
  }
  // Tiles turning over: face down before, face up after
  for (std::size_t i = 0; i < after.planets.size(); ++i)
    planetFlipping_.push_back(i < before.planets.size() && !before.planets[i].flipped &&
                              after.planets[i].flipped);
  for (std::size_t i = 0; i < after.zone.size(); ++i)
    siteFlipping_.push_back(i < before.zone.size() && !before.zone[i].flipped &&
                            after.zone[i].flipped);
}

ScratchPadd::Scene::Vector3 BoardMotion::shipAt(std::size_t ship, float t) const {
  if (ship >= shipTo_.size())
    return {};
  const float s = std::clamp(t, 0.0f, 1.0f);
  const auto &from = shipFrom_[ship], &to = shipTo_[ship];
  return from + (to - from) * s;
}

namespace {
std::optional<float> turnOf(const std::vector<bool> &flipping, std::size_t i, float t) {
  if (i >= flipping.size() || !flipping[i])
    return std::nullopt;
  return std::numbers::pi_v<float> * std::clamp(t, 0.0f, 1.0f);
}
} // namespace

std::optional<float> BoardMotion::planetTurn(std::size_t planet, float t) const {
  return turnOf(planetFlipping_, planet, t);
}

std::optional<float> BoardMotion::siteTurn(std::size_t site, float t) const {
  return turnOf(siteFlipping_, site, t);
}

} // namespace CosmicHex::Handheld
