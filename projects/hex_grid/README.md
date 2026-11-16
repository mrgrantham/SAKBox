# Hex grid

The maths of a hex-tile board: positions, neighbours, distances, rings, lines, rotation and reflection, the corners and edges between hexes, a hexagon-shaped board, paths around obstacles (hex to hex, or corner to corner along edges), and where hexes and corners are on screen. Written for Cosmic Hex, but nothing in it is about that game. Header-only, with no dependencies.

```cpp
#include <HexGrid/HexagonMap.hpp>
#include <HexGrid/Paths.hpp>

HexGrid::HexagonMap<Tile> board(5);             // 91 hexes, five from centre to edge
board[{2, -1}] = Tile::Planet;
int far = HexGrid::distance({2, -1}, {-3, 0});  // 5
for (HexGrid::Hex next : HexGrid::neighbours({2, -1})) ...

auto open = [&](HexGrid::Hex h) { return board.contains(h) && !blockaded(h); };
for (const HexGrid::Reach<HexGrid::Hex> &reach : HexGrid::reachable(ship, 3, open)) ...   // where a ship can go
std::vector<HexGrid::Hex> way = HexGrid::shortestPath(ship, planet, open);
```

## Positions

`Hex` (`Hex.hpp`) is a position in **axial coordinates** `(q, r)`, with the third cube coordinate `s = -q - r` when needed, the scheme Red Blob Games' [guide to hexagonal grids](https://www.redblobgames.com/grids/hexagons/) recommends. Everything on positions is integer arithmetic, so it gives the same results on every platform, the DS included, and works at compile time.

| | |
| --- | --- |
| `kDirections`, `direction(i)` | The six directions, counter-clockwise from +q |
| `neighbour(hex, i)`, `neighbours(hex)` | One neighbour, or all six in direction order |
| `distance(a, b)`, `length(hex)` | Steps between two hexes, or to the centre |
| `rotated(hex, turns, centre)` | Turned `turns` x 60 degrees counter-clockwise |
| `reflected(hex, centre)` | Mirrored across the line along the q axis |
| `ring(centre, radius)` | The hexes at exactly that distance, going round in order |
| `spiral(centre, radius)` | Every hex within the radius, ring by ring outwards |
| `line(a, b)` | The hexes on the straight line between, each a step from the last |

The directions are fixed, so the order of neighbours and of the hexes on a ring is too: anything built on them (a board, a search) comes out the same every time. Where a line runs exactly along the edge between two hexes, it always takes the same side: it's worked out in scaled integers, nudged off the edges the way the guide nudges its floats.

`Hex` can be compared, sorted (by q, then r) and hashed (`std::hash<HexGrid::Hex>`).

## Corners and edges

For games that build where hexes meet and move along their sides (as Starfarers and Catan do), `CornersAndEdges.hpp` names the points where three hexes meet (`Corner`) and the sides two share (`Edge`):

| | |
| --- | --- |
| `corner(hex, i)`, `corners(hex)` | Corner i of a hex, between its neighbours in directions i and i + 1, or all six |
| `edge(hex, i)`, `edges(hex)` | Edge i of a hex, the side towards its neighbour in direction i, or all six |
| `hexesTouching(corner)`, `hexesTouching(edge)` | The three hexes at a corner, or the two either side of an edge |
| `endpoints(edge)` | The two corners at its ends |
| `edgesMeeting(corner)` | The three edges that meet there |
| `neighbours(corner)` | The three corners an edge away (one step along edges) |
| `edgeBetween(a, b)` | The edge joining two corners, if they're neighbours |
| `everyCorner(hexes)`, `everyEdge(hexes)` | A board's corners or edges, each once, sorted |

Each corner is shared by three hexes and each edge by two, so to give each a single name, every hex owns two of its corners (0 and 1) and three of its edges (0, 1 and 2), and the rest belong to its neighbours: `corner(hex, i)` and `edge(hex, i)` take any of the six and give the owner's name, so the hexes around a corner all agree on what it's called. Corners and edges compare, sort and hash like `Hex`, and are integer arithmetic like the rest. A board of radius R has 6 x (R + 1)^2 corners.

## A hexagon-shaped board

`HexagonMap<T>` (`HexagonMap.hpp`) holds a value for every hex within a radius of the centre (1 + 3 x radius x (radius + 1) of them) in one array, row by row, so a board is cheap to copy (a bot copies it for every move it tries) and has no hashing. `contains`, `operator[]`, `hexes()` (spiral order), `neighboursOn(hex)` (the neighbours that are on the board).

## Paths

`Paths.hpp` moves pieces where each step costs the same, entering only the places a predicate allows (so blocked hexes, other pieces and the board's edge are the caller's rules). A place is a `Hex` (moving hex to hex) or a `Corner` (moving corner to corner along edges); the same functions do both:

- `reachable(from, maxSteps, canEnter)`: every place within that many steps, as `Reach<Place>{place, steps}`, with the fewest steps to each.
- `shortestPath(from, to, canEnter, maxSteps)`: the places of a shortest way, both ends included, or nothing if there's none.

Both are breadth-first searches that try neighbours in a fixed order, so the same board gives the same answer every time. `canEnter` says where a piece may pass; where it may stop can be stricter (a ship in Starfarers passes occupied corners but can't stop on one), so filter `reachable`'s result for that.

## On screen

`Layout` (`Layout.hpp`) places hexes in pixels or world units, pointy-topped or flat-topped, at any size and origin: `centre(hex)`, `corners(hex)`, `point(corner)` (where a `Corner` is), and `hexAt(point)` for a touch. This part uses floating point; it's for drawing and input, which needn't match to the last bit everywhere, while the game's rules stay on `Hex`.

## Building

Bazel: depend on `//projects/hex_grid`, and include `<HexGrid/...>`. It needs C++20.

A DS or 3DS app built with devkitPro's Makefiles: add `"//projects/hex_grid:sources": "hex_grid"` to the app's `extra_srcs`, and `hex_grid/include` to its Makefile's `INCLUDES`.

## Tests

| Test | What it checks |
| --- | --- |
| `Hex_test` | Neighbours one step away and in order; distances; six turns coming back round, each turn of a direction giving the next; reflection; rings and spirals holding exactly the right hexes, in order; lines going step by step, along a row exactly, and repeatable along edges; sorting and hashing |
| `CornersAndEdges_test` | Each corner touching its three hexes and each edge its two; one name for each, whichever hex asks; an edge's two ends; three edges and three neighbours at every corner; a board's corner and edge counts; corners on screen at the hexes' drawn corners |
| `HexagonMap_test` | Exactly the hexes within the radius; each with its own value; filled at the start; neighbours at corners and edges; copies being separate |
| `Paths_test` | Reach on an open board; a wall walked around, and out of reach in too few steps; no way, or too far, giving nothing; straight runs; repeatable; corner to corner along edges |
| `Layout_test` | Neighbours' centres evenly spaced in both orientations; corners at the size; points finding their hex |

```sh
bazel test //projects/hex_grid/...
```
