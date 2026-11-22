#include <CosmicHex/Handheld/BoardMotion.hpp>

#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <ScratchPadd/Motion/Motion.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace CosmicHex::Handheld {

BoardMotion::BoardMotion(const State &before, const State &after, const BoardMotion *last) {
  // Ships match one for one when no ship came or went: a move changes one
  // ship's corner and nothing else
  const bool sameShips =
      before.ships.size() == after.ships.size() &&
      std::equal(before.ships.begin(), before.ships.end(), after.ships.begin(),
                 [](const Ship &a, const Ship &b) { return a.owner == b.owner; });
  shipsCameOrWent_ = !sameShips;
  // The heading ship `i` of `after` had at the end of `last`: the same
  // ship's; when ships came or went, that of the ship of its owner's that
  // stood where it stands (a new ship, none: as its mesh points)
  const auto headingBefore = [&](std::size_t i) -> float {
    if (!last)
      return 0;
    std::optional<std::size_t> was;
    if (sameShips)
      was = i;
    else
      for (std::size_t j = 0; j < before.ships.size(); ++j)
        if (before.ships[j].owner == after.ships[i].owner &&
            before.ships[j].corner == after.ships[i].corner)
          was = j;
    return was && *was < last->headingAtRest_.size() ? last->headingAtRest_[*was] : 0;
  };
  for (std::size_t i = 0; i < after.ships.size(); ++i) {
    shipTo_.push_back(BoardPieces::at(after.ships[i].corner));
    shipFrom_.push_back(sameShips ? BoardPieces::at(before.ships[i].corner) : shipTo_.back());
    // Facing the way it was flying as it arrived, else as it was
    headingAtRest_.push_back(
        Flight{shipFrom_.back(), shipTo_.back(), 0}.arrivingHeading().value_or(headingBefore(i)));
  }
  // The blockade: swooping from the planet it was on to the next (or
  // dropping onto the first, from above)
  if (after.blockade >= 0) {
    const auto to =
        BoardPieces::centre(after.planets[static_cast<std::size_t>(after.blockade)].hex);
    const auto from =
        before.blockade >= 0
            ? BoardPieces::centre(before.planets[static_cast<std::size_t>(before.blockade)].hex)
            : to + ScratchPadd::Scene::Vector3{0, kDropHeight, 0};
    blockade_ = Flight{from, to, before.blockade >= 0 ? kSwoop : 0};
    blockadeMoves_ = before.blockade != after.blockade;
    const float was = last ? last->blockadeHeading_ : 0;
    blockadeHeading_ = blockadeMoves_ ? blockade_->arrivingHeading().value_or(was) : was;
  }
  // Stations built: in `after` and not in `before` as they are (a
  // station become a colony is built again)
  for (const Station &station : after.stations)
    stationBuilt_.push_back(std::find(before.stations.begin(), before.stations.end(), station) ==
                            before.stations.end());
  // Tiles turning over: face down before, face up after
  for (std::size_t i = 0; i < after.planets.size(); ++i)
    planetFlipping_.push_back(i < before.planets.size() && !before.planets[i].flipped &&
                              after.planets[i].flipped);
  for (std::size_t i = 0; i < after.zone.size(); ++i)
    siteFlipping_.push_back(i < before.zone.size() && !before.zone[i].flipped &&
                            after.zone[i].flipped);
}

std::optional<float> BoardMotion::Flight::arrivingHeading() const {
  // (the arc's last direction: the line less the bow's turn back in)
  const ScratchPadd::Scene::Vector3 way{to.x - from.x, 0, to.z - from.z};
  if (way.length() < 1e-4f)
    return std::nullopt;
  const ScratchPadd::Scene::Vector3 left{way.z, 0, -way.x};
  const ScratchPadd::Scene::Vector3 last = way - left * (kBow * std::numbers::pi_v<float>);
  return std::atan2(-last.z, last.x);
}

ScratchPadd::Scene::Vector3 BoardMotion::Flight::at(float t) const {
  const float s =
      ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::Gentle, std::clamp(t, 0.0f, 1.0f));
  // (bowing out to the flight's left, square to it on the table, and
  // hopping up off the table and down again)
  const ScratchPadd::Scene::Vector3 way = to - from;
  const ScratchPadd::Scene::Vector3 left{way.z, 0, -way.x};
  const float arc = std::sin(std::numbers::pi_v<float> * s);
  return from + way * s + left * (kBow * arc) + ScratchPadd::Scene::Vector3{0, hop * arc, 0};
}

ShipPose BoardMotion::Flight::pose(float t, float restingHeading) const {
  ShipPose pose;
  pose.at = at(t);
  pose.heading = restingHeading;
  const ScratchPadd::Scene::Vector3 way = to - from;
  const float length = way.length();
  if (length < 1e-4f || t <= 0 || t >= 1)
    return pose;
  // Its velocity and acceleration along the flight, from nearby moments
  constexpr float kH = 1.0f / 256;
  const auto before = at(t - kH), after = at(t + kH);
  const ScratchPadd::Scene::Vector3 velocity = (after - before) * (1 / (2 * kH));
  const ScratchPadd::Scene::Vector3 push = (after + before - pose.at * 2) * (1 / (kH * kH));
  if (std::hypot(velocity.x, velocity.z) > 1e-4f)
    pose.heading = std::atan2(-velocity.z, velocity.x); // (nose first)
  // The push in the ship's own axes (x its nose, z its right side)
  const float c = std::cos(pose.heading), s = std::sin(pose.heading);
  const float forward = push.x * c - push.z * s;
  const float sideways = push.x * s + push.z * c;
  // Burning while it speeds up, as hard as it pushes (the Gentle curve's
  // hardest push is near 6 lengths a t squared); swivelled against the
  // sideways part (the flame goes the other way from the push)
  pose.flame = std::clamp(forward / (5 * length), 0.0f, 1.0f);
  if (pose.flame > 0)
    pose.gimbal = std::clamp(std::atan2(-sideways, forward), -kMostGimbal, kMostGimbal);
  return pose;
}

ShipPose BoardMotion::ship(std::size_t ship, float t) const {
  if (ship >= shipTo_.size())
    return {};
  return Flight{shipFrom_[ship], shipTo_[ship], 0}.pose(t, headingAtRest_[ship]);
}

float BoardMotion::restingHeading(std::size_t ship) const {
  return ship < headingAtRest_.size() ? headingAtRest_[ship] : 0;
}

std::optional<ShipPose> BoardMotion::blockade(float t) const {
  if (!blockade_)
    return std::nullopt;
  if (!blockadeMoves_)
    return blockade_->pose(1, blockadeHeading_);
  return blockade_->pose(t, blockadeHeading_);
}

bool BoardMotion::anything() const {
  if (shipsCameOrWent_ || blockadeMoves_)
    return true;
  for (std::size_t i = 0; i < shipTo_.size(); ++i)
    if ((shipTo_[i] - shipFrom_[i]).length() > 1e-4f)
      return true;
  const auto any = [](const std::vector<bool> &flags) {
    return std::find(flags.begin(), flags.end(), true) != flags.end();
  };
  return any(stationBuilt_) || any(planetFlipping_) || any(siteFlipping_);
}

DustRing dustRing(float dust, float spreadsTo) {
  const float spreading = ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseOut, dust);
  return {
      .spread = 1 + (spreadsTo - 1) * spreading,
      .material = {.shading = ScratchPadd::Scene::Material::Shading::Unlit,
                   .colour = {236, 226, 204},
                   .alpha = static_cast<std::uint8_t>(26 * (1 - std::clamp(dust, 0.0f, 1.0f)))}};
}

std::array<RippleRing, 2> spaceRipple(float ripple) {
  constexpr float kSecondAfter = 0.3f;                   // (of the ripple's time)
  constexpr float kTurn = std::numbers::pi_v<float> / 3; // (a sixth: the hexagon turns visibly)
  std::array<RippleRing, 2> rings{};
  for (std::size_t i = 0; i < rings.size(); ++i) {
    const float after = kSecondAfter * static_cast<float>(i);
    const float t = std::clamp((ripple - after) / (1 - kSecondAfter), 0.0f, 1.0f);
    const float out = ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::EaseOut, t);
    const float shows = std::min(1.0f, t * 6) * (1 - t); // (quickly in, slowly out)
    rings[i] = {.spread = 0.55f + 0.75f * out,
                .turn = (i == 0 ? kTurn : -kTurn) * out,
                .material = {.shading = ScratchPadd::Scene::Material::Shading::Unlit,
                             .colour = {176, 160, 255},
                             .alpha = static_cast<std::uint8_t>(std::lround(28 * shows))}};
  }
  return rings;
}

Landing BoardMotion::landing(std::size_t station, float t) const {
  Landing landing;
  if (station >= stationBuilt_.size() || !stationBuilt_[station] || t >= kLandingEnds)
    return landing;
  if (t < kLanded) {
    // Falling: faster and faster, as under gravity
    const float falling = std::max(0.0f, t) / kLanded;
    landing.drop = kDropHeight * (1 - falling * falling);
    return landing;
  }
  // Down: squashed at first, springing back (Settle's overshoot), the dust
  // spreading out and fading
  const float since = (t - kLanded) / (kLandingEnds - kLanded);
  const float spring =
      ScratchPadd::Motion::along(ScratchPadd::Motion::Curve::Settle, std::min(1.0f, since * 3));
  landing.squash = 0.7f + 0.3f * spring;
  landing.dust = since;
  return landing;
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
