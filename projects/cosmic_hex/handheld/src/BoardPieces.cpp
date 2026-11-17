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

BoardPieces::BoardPieces() {
  // A hexagon a little inside its hex, so the space between shows as a grid
  const auto corners = kLayout.corners(Hex{0, 0});
  const Scene::Vector3 middle{0, 0, 0};
  for (int i = 0; i < 6; ++i) {
    const auto a = corners[static_cast<std::size_t>(i)],
               b = corners[static_cast<std::size_t>((i + 1) % 6)];
    hexTile.triangle(middle, onTable({b.x * 0.94f, b.y * 0.94f}),
                     onTable({a.x * 0.94f, a.y * 0.94f}));
    // and its rim, a thin ring for the chosen hex
    highlight.quad(
        onTable({a.x * 0.98f, a.y * 0.98f}, 0.02f), onTable({b.x * 0.98f, b.y * 0.98f}, 0.02f),
        onTable({b.x * 0.86f, b.y * 0.86f}, 0.02f), onTable({a.x * 0.86f, a.y * 0.86f}, 0.02f));
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

Scene::Vector3 BoardPieces::centre(Hex hex) { return onTable(kLayout.centre(hex)); }

Scene::Vector3 BoardPieces::at(Corner corner) { return onTable(kLayout.point(corner)); }

Scene::Transform BoardPieces::onHex(Hex hex) { return Scene::Transform::translation(centre(hex)); }

Scene::Transform BoardPieces::onCorner(Corner corner) {
  return Scene::Transform::translation(at(corner));
}

} // namespace CosmicHex::Handheld
