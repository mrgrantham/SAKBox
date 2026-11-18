// The DS's display lists, read back command by command (GBATEK's packed
// format), with Scalar as float and as Q16.16 (the DS's)
#include <DisplayList.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <vector>

using namespace ScratchPadd::Scene;
using namespace ScratchPadd::Scene::DisplayList;

namespace {

// How many parameter words each command used takes
int parameterCount(std::uint8_t command) {
  switch (command) {
  case KVertex16:
    return 2;
  case KColour:
  case KTexCoord:
  case KNormal:
  case KDiffuseAmbient:
  case KSpecularEmission:
  case KBegin:
    return 1;
  default:
    return 0;
  }
}

struct Sent {
  std::uint8_t command;
  std::vector<std::uint32_t> parameters;
};

// The commands in a display list, in the order the DS runs them
std::vector<Sent> unpack(const std::vector<std::uint32_t> &list) {
  EXPECT_EQ(list.at(0), list.size() - 1) << "the first word counts the rest";
  std::vector<Sent> sent;
  std::size_t at = 1;
  while (at < list.size()) {
    const std::uint32_t word = list[at++];
    for (int slot = 0; slot < 4; ++slot) {
      const auto command = static_cast<std::uint8_t>(word >> (8 * slot));
      if (command == 0)
        continue;
      Sent s{command, {}};
      for (int p = 0; p < parameterCount(command); ++p)
        s.parameters.push_back(list.at(at++));
      sent.push_back(s);
    }
  }
  return sent;
}

int count(const std::vector<Sent> &sent, std::uint8_t command) {
  int n = 0;
  for (const Sent &s : sent)
    n += s.command == command;
  return n;
}

// A 4.12 coordinate back from a vertex's parameters
std::int16_t vertexX(const Sent &s) { return static_cast<std::int16_t>(s.parameters[0] & 0xFFFF); }
std::int16_t vertexY(const Sent &s) { return static_cast<std::int16_t>(s.parameters[0] >> 16); }
std::int16_t vertexZ(const Sent &s) { return static_cast<std::int16_t>(s.parameters[1] & 0xFFFF); }

// A mesh's only display list (all its polygons one alpha)
std::vector<std::uint32_t> onlyList(const std::vector<Packed> &lists) {
  EXPECT_EQ(lists.size(), 1u);
  return lists.empty() ? std::vector<std::uint32_t>{} : lists[0].words;
}

} // namespace

TEST(DisplayListTest, ConvertsNumbersAsTheDsHasThem) {
  EXPECT_EQ(toFixed(Scalar(1.5f), 12), 6144);
  EXPECT_EQ(toFixed(Scalar(-0.25f), 12), -1024);
  EXPECT_EQ(toFixed(Scalar(1), 9), 512);
  // A normal's components: 1.9, just under 1 at most, 10 bits each
  const std::uint32_t up = packNormal({0, 1, 0});
  EXPECT_EQ(up & 0x3FF, 0u);
  EXPECT_EQ((up >> 10) & 0x3FF, 511u);
  EXPECT_EQ(packNormal({-1, 0, 0}) & 0x3FF, 0x200u); // -512
  EXPECT_EQ(rgb15({255, 0, 8}), 31 | (1 << 10));
  EXPECT_EQ(multiply({255, 128, 0}, {128, 255, 255}), (Colour{128, 128, 0, 255}));
}

// An unlit box, made in code: its faces joined into quads when it's
// prepared, a colour only when it changes, and every corner where the mesh
// has it, in 4.12
TEST(DisplayListTest, UnlitMeshesSendColoursAndVertices) {
  MeshBuilder builder;
  builder.box({0, 0, 0}, {1, 0.5f, 2}, Colour{255, 0, 0});
  const Source source = prepare(builder.data());
  EXPECT_EQ(source.scaleShift, 0);
  ASSERT_EQ(source.corners.size(), 24u); // 6 quads

  const std::vector<Sent> sent =
      unpack(onlyList(pack(source, {.lit = false, .colour = {255, 255, 255}})));
  ASSERT_FALSE(sent.empty());
  EXPECT_EQ(sent[0].command, KBegin);
  EXPECT_EQ(sent[0].parameters[0], 1u); // quads
  EXPECT_EQ(count(sent, KBegin), 1);
  EXPECT_EQ(count(sent, KVertex16), 24);
  EXPECT_EQ(count(sent, KColour), 1); // one colour, sent once
  EXPECT_EQ(count(sent, KNormal), 0);
  EXPECT_EQ(sent[1].parameters[0], rgb15({255, 0, 0}));
  for (const Sent &s : sent) {
    if (s.command != KVertex16)
      continue;
    EXPECT_EQ(std::abs(vertexX(s)), 4096);
    EXPECT_EQ(std::abs(vertexY(s)), 2048);
    EXPECT_EQ(std::abs(vertexZ(s)), 8192);
  }

  // The material's colour multiplies the vertex colours
  const std::vector<Sent> tinted =
      unpack(onlyList(pack(source, {.lit = false, .colour = {128, 255, 255}})));
  EXPECT_EQ(tinted[1].parameters[0], rgb15({128, 0, 0}));
}

// Lit: each colour change sets the material's diffuse colour and its
// emission (the colour times the ambient light), then a normal; a normal is
// sent again only when it changes (a box's face shares one)
TEST(DisplayListTest, LitMeshesSendMaterialsAndNormals) {
  MeshBuilder builder;
  builder.quad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, Colour{200, 100, 0});
  builder.quad({0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, Colour{0, 0, 255});
  const Source source = prepare(builder.data());
  const Variant variant{.lit = true, .colour = {255, 255, 255}, .ambient = {128, 128, 128}};
  const std::vector<Sent> sent = unpack(onlyList(pack(source, variant)));
  EXPECT_EQ(count(sent, KVertex16), 8); // two quads
  EXPECT_EQ(count(sent, KColour), 0);
  EXPECT_EQ(count(sent, KDiffuseAmbient), 2);
  EXPECT_EQ(count(sent, KSpecularEmission), 2);
  EXPECT_EQ(count(sent, KNormal), 2); // one a quad: they face the same way, but the colour changed

  // In order: the material, its emission, the normal, then the vertices
  ASSERT_GE(sent.size(), 4u);
  EXPECT_EQ(sent[1].command, KDiffuseAmbient);
  EXPECT_EQ(sent[1].parameters[0], rgb15({200, 100, 0}));
  EXPECT_EQ(sent[2].command, KSpecularEmission);
  EXPECT_EQ(sent[2].parameters[0] >> 16, rgb15(multiply({200, 100, 0}, {128, 128, 128})));
  EXPECT_EQ(sent[3].command, KNormal);
  EXPECT_EQ(sent[3].parameters[0], packNormal({0, 0, 1}));
  EXPECT_EQ(sent[4].command, KVertex16);
}

// A mesh bigger than 4.12's +-8 is stored smaller by a power of two, which
// the renderer scales back up
TEST(DisplayListTest, BigMeshesAreScaledToFit) {
  MeshBuilder builder;
  builder.box({0, 0, 0}, {20, 1, 1});
  const Source source = prepare(builder.data());
  EXPECT_EQ(source.scaleShift, 2); // 20 / 4 = 5, within 8
  for (const Source::Corner &corner : source.corners) {
    EXPECT_EQ(std::abs(corner.x), 5 * 4096);
    EXPECT_EQ(std::abs(corner.y), 1024);
  }
}

// The packed words: four commands a word, then their parameters in order
TEST(DisplayListTest, PacksFourCommandsAWord) {
  MeshBuilder builder;
  builder.triangle({0, 0, 0}, {1, 0, 0}, {0, 1, 0});
  const std::vector<std::uint32_t> list = onlyList(pack(prepare(builder.data()), {.lit = false}));
  // BEGIN, COLOR, VTX, VTX | VTX: two command words, 1 + 1 + 2 x 3 parameters
  ASSERT_EQ(list.size(), 1u + 2 + 8);
  EXPECT_EQ(list[1], KBegin | (KColour << 8) | (KVertex16 << 16) | (KVertex16 << 24));
  EXPECT_EQ(list[8], static_cast<std::uint32_t>(KVertex16));
}

// A cooked mesh's runs are kept as they are: each starts its kind of
// polygon (a strip here), its corners in order
TEST(DisplayListTest, CookedRunsAreDrawnAsGiven) {
  const Vertex vertices[] = {{{0, 0, 0}, {0, 0, 1}, {}},
                             {{1, 0, 0}, {0, 0, 1}, {}},
                             {{0, 1, 0}, {0, 0, 1}, {}},
                             {{1, 1, 0}, {0, 0, 1}, {}}};
  const std::uint32_t indices[] = {0, 1, 2, 3};
  const Primitive runs[] = {{Primitive::Kind::TriangleStrip, 31, 4}};
  const Source source = prepare({vertices, indices, runs});
  ASSERT_EQ(source.runs.size(), 1u);
  EXPECT_EQ(source.runs[0].kind, Primitive::Kind::TriangleStrip);
  const std::vector<Sent> sent = unpack(onlyList(pack(source, {.lit = false})));
  EXPECT_EQ(sent[0].parameters[0], 2u); // a triangle strip
  EXPECT_EQ(count(sent, KVertex16), 4);
  EXPECT_EQ(vertexX(sent.back()), 4096);
  EXPECT_EQ(vertexY(sent.back()), 4096);
}

// Polygons of different alphas go in lists of their own, the most opaque
// first: the DS sets a polygon's alpha before its list is called
TEST(DisplayListTest, EachAlphaHasItsOwnList) {
  MeshBuilder builder;
  builder.quad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, Colour{255, 255, 255, 255});
  builder.quad({0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}, Colour{255, 255, 255, 128});
  const std::vector<Packed> lists = pack(prepare(builder.data()), {.lit = false});
  ASSERT_EQ(lists.size(), 2u);
  EXPECT_EQ(lists[0].alpha, 31);
  EXPECT_EQ(lists[1].alpha, 16); // 128 of 255, in 32 steps
  for (const Packed &list : lists)
    EXPECT_EQ(count(unpack(list.words), KVertex16), 4);
}

// Toon shading's colours from a texture: the mesh's distinct colours are its
// palette, each corner points at the middle of its colour's texel, the
// vertices are lit as white, and the texture is the palette times the
// material's colour
TEST(DisplayListTest, ToonColoursComeFromATexture) {
  MeshBuilder builder;
  builder.box({0, 0, 0}, {1, 1, 1}, Colour{255, 0, 0});
  builder.box({3, 0, 0}, {1, 1, 1}, Colour{0, 0, 255});
  const Source source = prepare(builder.data());
  ASSERT_EQ(source.palette.size(), 2u);
  EXPECT_EQ(source.palette[0], (Colour{255, 0, 0, 255}));
  EXPECT_EQ(source.palette[1], (Colour{0, 0, 255, 255}));

  const std::vector<Sent> sent = unpack(onlyList(pack(
      source, {.lit = true, .colour = {255, 255, 255}, .ambient = {0, 0, 0}, .textured = true})));
  // Two texels used, each sent once as the boxes' corners come in turn
  std::vector<std::uint32_t> coordinates;
  for (const Sent &s : sent) {
    if (s.command == KTexCoord)
      coordinates.push_back(s.parameters[0]);
    if (s.command == KDiffuseAmbient) { // (lit as white)
      EXPECT_EQ(s.parameters[0], rgb15({255, 255, 255}));
    }
  }
  EXPECT_EQ(coordinates, (std::vector<std::uint32_t>{8 | (8 << 16), (16 + 8) | (8 << 16)}));

  const std::vector<std::uint16_t> texture = textureFor(source, {255, 128, 255});
  ASSERT_EQ(texture.size(), static_cast<std::size_t>(kTexels));
  EXPECT_EQ(texture[0], rgb15({255, 0, 0}) | 0x8000);
  EXPECT_EQ(texture[1], rgb15({0, 0, 255}) | 0x8000);
  EXPECT_EQ(texture[2], 0x8000); // (unused: black)
}

// More colours than texels: the rest share the nearest
TEST(DisplayListTest, APaletteHoldsAtMostItsTexels) {
  MeshBuilder builder;
  for (int i = 0; i < kTexels + 6; ++i)
    builder.triangle({0, 0, 0}, {1, 0, 0}, {0, 1, 0},
                     Colour{static_cast<std::uint8_t>(i % 8 * 32),
                            static_cast<std::uint8_t>(i / 8 % 8 * 32),
                            static_cast<std::uint8_t>(i / 64 * 32)}); // (distinct in 15 bits)
  const Source source = prepare(builder.data());
  EXPECT_EQ(source.palette.size(), static_cast<std::size_t>(kTexels));
  for (const Source::Corner &corner : source.corners)
    EXPECT_LT(corner.texel, kTexels);
}

// An image texture: each corner sends its Vertex::uv as texels of the
// image's size (12.4), the colours white (the image gives them)
TEST(DisplayListTest, ImageTexturesSendTheirCoordinates) {
  MeshBuilder builder;
  builder.quad({0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {200, 100, 50},
               {TexCoord{0, 1}, TexCoord{1, 1}, TexCoord{1, 0}, TexCoord{0, 0}});
  const Source source = prepare(builder.data());
  const auto sent =
      unpack(onlyList(pack(source, {.lit = false, .imageWidth = 32, .imageHeight = 16})));
  std::vector<std::uint32_t> coordinates;
  for (const Sent &s : sent) {
    if (s.command == KTexCoord)
      coordinates.push_back(s.parameters[0]);
    if (s.command == KColour) {
      EXPECT_EQ(s.parameters[0], rgb15({255, 255, 255})); // (white: the image's colours)
    }
  }
  ASSERT_FALSE(coordinates.empty());
  // (0, 1) is the image's bottom left: s 0, t 16 texels; (1, 0) its top
  // right: s 32, t 0 (each x 16: 12.4). The DS's quad may start anywhere
  const auto has = [&](std::uint32_t c) {
    return std::find(coordinates.begin(), coordinates.end(), c) != coordinates.end();
  };
  EXPECT_TRUE(has(0u | ((16u * 16u) << 16)));
  EXPECT_TRUE(has(32u * 16u));
}

// The overlay's triangles' depths (round 92, the user's: the roll's hexagon
// lost a slice, as one step apart the DS's depth buffer rounded neighbouring
// triangles together, and a see-through one over an opaque one failed the
// depth test; melonDS showed it too, and 16 steps apart it was gone): each
// nearer than the one before, at least kOverlayDepthStep apart for any
// frame's worth of triangles, in a 16-bit number's range and off the near
// plane
TEST(DisplayListTest, OverlayTrianglesAreFarEnoughApartForTheDepthBuffer) {
  using DisplayList::kOverlayDepthStep;
  using DisplayList::overlayDepth;
  for (const int count : {1, 2, 12, 100, 1000, 1999}) {
    for (int t = 0; t < count; ++t) {
      const int depth = overlayDepth(t, count);
      EXPECT_LT(depth, 0) << t << " of " << count; // (in front of the camera)
      EXPECT_GE(depth, -32768) << t << " of " << count;
      if (t > 0) {
        EXPECT_GE(depth - overlayDepth(t - 1, count), kOverlayDepthStep) << t << " of " << count;
      }
    }
  }
  // Even more than a frame can draw (the DS: 2048 polygons) stays in range,
  // each still nearer than the last
  for (int t = 1; t < 5000; ++t) {
    EXPECT_GT(overlayDepth(t, 5000), overlayDepth(t - 1, 5000));
    EXPECT_GE(overlayDepth(t - 1, 5000), -32768);
  }
}
