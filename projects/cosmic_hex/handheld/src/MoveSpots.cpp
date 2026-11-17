#include <CosmicHex/Handheld/MoveSpots.hpp>

#include <HexGrid/CornersAndEdges.hpp>

#include <algorithm>

namespace CosmicHex::Handheld {

namespace {

std::optional<Corner> cornerAt(int number) {
  const auto &corners = boardCorners();
  if (number < 0 || number >= static_cast<int>(corners.size()))
    return std::nullopt;
  return corners[static_cast<std::size_t>(number)];
}

} // namespace

MoveSpot spotOf(const State &state, const Action &move) {
  using Kind = Action::Kind;
  MoveSpot spot;
  const auto station = [&](int i) {
    if (i >= 0 && i < static_cast<int>(state.stations.size()))
      spot.corners.push_back(state.stations[static_cast<std::size_t>(i)].at);
  };
  const auto ship = [&](int i) {
    if (i >= 0 && i < static_cast<int>(state.ships.size()))
      spot.corners.push_back(state.ships[static_cast<std::size_t>(i)].corner);
  };
  const auto corner = [&](int number) {
    if (const auto c = cornerAt(number))
      spot.corners.push_back(*c);
  };
  switch (move.kind) {
  case Kind::BuildShip:
  case Kind::PlayShipyard:
  case Kind::BuildColony:
    station(move.a);
    break;
  case Kind::Move:
    ship(move.a);
    corner(move.b);
    break;
  case Kind::Settle:
    ship(move.a);
    break;
  case Kind::PlaceColony:
  case Kind::PlaceShip:
    corner(move.a);
    break;
  case Kind::MoveBlockade:
    if (move.a >= 0 && move.a < static_cast<int>(state.planets.size()))
      spot.hex = state.planets[static_cast<std::size_t>(move.a)].hex;
    break;
  default:
    break;
  }
  return spot;
}

bool concerns(const State &state, const Action &move, const BoardPicture::Spot &touched) {
  const MoveSpot spot = spotOf(state, move);
  if (touched.corner)
    return std::find(spot.corners.begin(), spot.corners.end(), *touched.corner) !=
           spot.corners.end();
  if (spot.hex && *spot.hex == touched.hex)
    return true;
  return std::any_of(spot.corners.begin(), spot.corners.end(), [&](const Corner &c) {
    const auto hexes = HexGrid::hexesTouching(c);
    return std::find(hexes.begin(), hexes.end(), touched.hex) != hexes.end();
  });
}

} // namespace CosmicHex::Handheld
