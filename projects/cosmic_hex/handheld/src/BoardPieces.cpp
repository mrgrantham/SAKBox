#include <CosmicHex/Handheld/BoardPieces.hpp>

#include <HexGrid/Layout.hpp>
#include <SeededRandom/Generator.hpp>

#include <array>
#include <cmath>

namespace CosmicHex::Handheld {

namespace {

// Pointy-topped hexes, 1 from centre to corner, (0, 0) at the table's centre
const HexGrid::Layout kLayout{};

Scene::Vector3 onTable(HexGrid::Point point, float height = 0) {
  return {point.x, height, point.y};
}

} // namespace

Scene::Colour BoardColours::ofKind(PlanetKind kind) {
  switch (kind) {
  case PlanetKind::AsteroidBelt:
    return {170, 150, 130};
  case PlanetKind::LushPlanet:
    return {110, 200, 110};
  case PlanetKind::GasGiant:
    return {235, 150, 80};
  case PlanetKind::IceMoon:
    return {170, 215, 245};
  case PlanetKind::Nebula:
    return {185, 130, 230};
  case PlanetKind::Home:
    return kHome;
  case PlanetKind::Moon:
    return {205, 205, 215};
  }
  return kFaceDown;
}

Scene::Colour BoardColours::ofPlayer(int player) {
  static constexpr std::array<Scene::Colour, kMaxPlayers> kPlayers = {
      Scene::Colour{235, 70, 70}, Scene::Colour{70, 140, 245}, Scene::Colour{245, 245, 245},
      Scene::Colour{250, 200, 40}};
  return kPlayers[static_cast<std::size_t>(player) % kPlayers.size()];
}

Scene::Colour BoardColours::ofFace(Art::Face face) {
  using Art::Face;
  switch (face) {
  case Face::PlanetMinerals:
    return ofKind(PlanetKind::AsteroidBelt);
  case Face::PlanetFood:
    return ofKind(PlanetKind::LushPlanet);
  case Face::PlanetFuel:
    return ofKind(PlanetKind::GasGiant);
  case Face::PlanetWater:
    return ofKind(PlanetKind::IceMoon);
  case Face::PlanetCrystals:
    return ofKind(PlanetKind::Nebula);
  case Face::PlanetMoon:
    return ofKind(PlanetKind::Moon);
  case Face::Home:
    return {47, 143, 216}; // (its ocean blue)
  case Face::HomeExhaustedMinerals:
  case Face::HomeExhaustedFood:
  case Face::HomeExhaustedFuel:
  case Face::HomeExhaustedWater:
  case Face::HomeExhaustedCrystals:
    return {142, 152, 166}; // (its metal grey)
  case Face::FindOutpost:
    return kOutpost;
  case Face::FindSalvage:
  case Face::FindRuins:
  case Face::FindPirates:
  case Face::FindNothing:
    return kFound;
  case Face::PlanetFaceDown:
    return kFaceDown;
  case Face::FindFaceDown:
    return kZone;
  }
  return kFaceDown;
}

BoardPieces::BoardPieces() {
  // A hexagon a little inside its hex, so the space between shows as a grid
  const auto corners = kLayout.corners(Hex{0, 0});
  const Scene::Vector3 middle{0, 0, 0};
  for (int i = 0; i < 6; ++i) {
    const auto a = corners[static_cast<std::size_t>(i)],
               b = corners[static_cast<std::size_t>((i + 1) % 6)];
    // Each corner where the tile's face (Art.hpp) has it: the hex, 47% of
    // the square from its centre to a corner, v down the board (+z)
    // (kTileInset of the hex: the rest is the grid's, gapsBetween)
    const auto uv = [](HexGrid::Point p) {
      return Scene::TexCoord{0.5f + p.x * 0.47f, 0.5f + p.y * 0.47f};
    };
    hexTile.triangle(middle, onTable({b.x * kTileInset, b.y * kTileInset}),
                     onTable({a.x * kTileInset, a.y * kTileInset}), {},
                     {Scene::TexCoord{0.5f, 0.5f}, uv(b), uv(a)});
    // and its rim, a thin ring for the chosen hex
    hexRing.quad(onTable({a.x * 0.98f, a.y * 0.98f}, kRingLift),
                 onTable({b.x * 0.98f, b.y * 0.98f}, kRingLift),
                 onTable({b.x * 0.86f, b.y * 0.86f}, kRingLift),
                 onTable({a.x * 0.86f, a.y * 0.86f}, kRingLift));
  }
  // A small hexagonal ring round a corner, at the rings' height (// the user's: a touch marks the
  // corner, not the tile)
  {
    constexpr float kOuter = 0.24f, kInner = 0.15f;
    const auto corners = kLayout.corners(Hex{});
    for (int i = 0; i < 6; ++i) {
      const auto a = corners[static_cast<std::size_t>(i)],
                 b = corners[static_cast<std::size_t>((i + 1) % 6)];
      cornerRing.quad(onTable({a.x * kOuter, a.y * kOuter}, kRingLift),
                      onTable({b.x * kOuter, b.y * kOuter}, kRingLift),
                      onTable({b.x * kInner, b.y * kInner}, kRingLift),
                      onTable({a.x * kInner, a.y * kInner}, kRingLift));
    }
  }
  // The number chip: a square just above the tile (kChipLift), kChipDown towards the
  // viewer, its four corners turning the same way as the tile's (so it faces
  // up as the tile does), the chip's texture across it
  {
    const HexGrid::Point centre{0, kChipDown};
    const float pi = 3.14159265f;
    std::array<HexGrid::Point, 4> square{};
    for (std::size_t k = 0; k < 4; ++k) {
      const float angle = pi / 4 + pi / 2 * static_cast<float>(k);
      square[k] = {centre.x + kChipRadius * std::sqrt(2.0f) * std::cos(angle),
                   centre.y + kChipRadius * std::sqrt(2.0f) * std::sin(angle)};
    }
    const auto uv = [&](HexGrid::Point p) {
      return Scene::TexCoord{0.5f + (p.x - centre.x) / (2 * kChipRadius),
                             0.5f + (p.y - centre.y) / (2 * kChipRadius)};
    };
    for (std::size_t k = 0; k < 4; ++k) {
      const auto a = square[k], b = square[(k + 1) % 4];
      numberChip.triangle(onTable(centre, kChipLift), onTable(b, kChipLift), onTable(a, kChipLift),
                          {}, {uv(centre), uv(b), uv(a)});
    }
  }
  station.box({0, 0.2f, 0}, {0.2f, 0.2f, 0.2f});
  colony.box({0, 0.3f, 0}, {0.24f, 0.3f, 0.24f});
  colony.box({0, 0.68f, 0}, {0.12f, 0.08f, 0.12f});
  // The ship: a flat dart, nose along +x
  const Scene::Vector3 nose{0.45f, 0.15f, 0}, left{-0.3f, 0.15f, -0.26f},
      right{-0.3f, 0.15f, 0.26f}, top{-0.18f, 0.36f, 0};
  ship.triangle(nose, left, top).triangle(nose, top, right).triangle(left, right, top);
  ship.triangle(nose, right, left);
  // Stars: about 160 across a disc 22 wide, 1 below the table, faint and
  // a little bluish, in a few brightnesses
  SeededRandom::Generator random(0x57A25);
  for (int i = 0; i < 160; ++i) {
    const float angle = static_cast<float>(random.below(3600)) / 3600.0f * 6.2831853f;
    const float radius = 3 + static_cast<float>(random.below(1900)) / 100.0f;
    const float x = radius * std::cos(angle), z = radius * std::sin(angle), size = 0.035f;
    const auto level = static_cast<std::uint8_t>(90 + random.below(80));
    const Scene::Colour colour{level, level, static_cast<std::uint8_t>(level + 20)};
    stars.quad({x - size, -1, z + size}, {x + size, -1, z + size}, {x + size, -1, z - size},
               {x - size, -1, z - size}, colour);
  }
}

Art::Face faceOf(const Planet &planet) {
  using Art::Face;
  if (planet.homeOf >= 0)
    return Face::Home;
  if (!planet.flipped)
    return Face::PlanetFaceDown;
  const bool exhausted = planet.formerHomeOf >= 0;
  switch (planet.kind) {
  case PlanetKind::AsteroidBelt:
    return exhausted ? Face::HomeExhaustedMinerals : Face::PlanetMinerals;
  case PlanetKind::LushPlanet:
    return exhausted ? Face::HomeExhaustedFood : Face::PlanetFood;
  case PlanetKind::GasGiant:
    return exhausted ? Face::HomeExhaustedFuel : Face::PlanetFuel;
  case PlanetKind::IceMoon:
    return exhausted ? Face::HomeExhaustedWater : Face::PlanetWater;
  case PlanetKind::Nebula:
    return exhausted ? Face::HomeExhaustedCrystals : Face::PlanetCrystals;
  case PlanetKind::Moon:
    return Face::PlanetMoon;
  case PlanetKind::Home:
    return Face::Home;
  }
  return Face::PlanetFaceDown;
}

int chipOf(const Planet &planet) {
  if (!planet.flipped || planet.kind == PlanetKind::Moon || planet.kind == PlanetKind::Home)
    return 0;
  return Art::chipRecipeOf(planet.number).empty() ? 0 : planet.number;
}

Art::Face faceOf(const ZoneSite &site) {
  using Art::Face;
  if (!site.flipped)
    return Face::FindFaceDown;
  switch (site.find) {
  case Find::Salvage:
    return Face::FindSalvage;
  case Find::Ruins:
    return Face::FindRuins;
  case Find::AlienStation:
    return Face::FindOutpost;
  case Find::PirateHideout:
    return Face::FindPirates;
  case Find::Nothing:
    return Face::FindNothing;
  }
  return Face::FindNothing;
}

namespace {
// Each hex's centre and its two corners (0 and 1, CornersAndEdges.hpp) on
// the table, worked out once and kept: the layout is in floats, which the
// DS has no hardware for, and every tile, chip, station and ship asks each
// frame. A board's radius is 4; the rest is for the corners,
// gaps and rings just past its edge.
constexpr int kKeptRadius = 6;
constexpr int kKeptSide = 2 * kKeptRadius + 1;
struct KeptPlaces {
  // [q][r]: the centre, then corner 0, then corner 1
  std::array<std::array<std::array<Scene::Vector3, 3>, kKeptSide>, kKeptSide> at{};
  KeptPlaces() {
    for (int q = 0; q < kKeptSide; ++q)
      for (int r = 0; r < kKeptSide; ++r) {
        const Hex hex{q - kKeptRadius, r - kKeptRadius};
        auto &places = at[static_cast<std::size_t>(q)][static_cast<std::size_t>(r)];
        places[0] = onTable(kLayout.centre(hex));
        places[1] = onTable(kLayout.point(Corner{hex, 0}));
        places[2] = onTable(kLayout.point(Corner{hex, 1}));
      }
  }
};
// Hex `hex`'s kept places, or none if it's beyond them
const std::array<Scene::Vector3, 3> *keptAt(Hex hex) {
  static const KeptPlaces kept; // (made on first use, once, whatever the thread)
  const int q = hex.q + kKeptRadius, r = hex.r + kKeptRadius;
  if (q < 0 || r < 0 || q >= kKeptSide || r >= kKeptSide)
    return nullptr;
  return &kept.at[static_cast<std::size_t>(q)][static_cast<std::size_t>(r)];
}
} // namespace

Scene::Vector3 BoardPieces::centre(Hex hex) {
  if (const auto *kept = keptAt(hex))
    return (*kept)[0];
  return onTable(kLayout.centre(hex));
}

Scene::Vector3 BoardPieces::at(Corner corner) {
  if (const auto *kept = keptAt(corner.hex); kept && (corner.index == 0 || corner.index == 1))
    return (*kept)[static_cast<std::size_t>(1 + corner.index)];
  return onTable(kLayout.point(corner));
}

Scene::Transform BoardPieces::onHex(Hex hex) { return Scene::Transform::translation(centre(hex)); }

Scene::MeshBuilder BoardPieces::gapsBetween(const std::vector<Hex> &tiles) {
  Scene::MeshBuilder gaps;
  for (Hex hex : tiles) {
    const auto corners = kLayout.corners(hex);
    const HexGrid::Point middle = kLayout.centre(hex);
    // A corner `share` of the way out from the hex's centre
    const auto out = [&](int i, float share) {
      const HexGrid::Point corner = corners[static_cast<std::size_t>(i % 6)];
      return onTable(
          {middle.x + (corner.x - middle.x) * share, middle.y + (corner.y - middle.y) * share});
    };
    // Each side's strip, turned the same way as the tiles' triangles
    for (int i = 0; i < 6; ++i) {
      gaps.triangle(out(i, kTileInset), out(i + 1, 1), out(i, 1));
      gaps.triangle(out(i, kTileInset), out(i + 1, kTileInset), out(i + 1, 1));
    }
  }
  return gaps;
}

Scene::Transform BoardPieces::onCorner(Corner corner) {
  return Scene::Transform::translation(at(corner));
}

} // namespace CosmicHex::Handheld
