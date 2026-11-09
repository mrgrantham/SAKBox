// The DS's display lists, read back command by command (GBATEK's packed
// format), with Scalar as float and as Q16.16 (the DS's)
#include <DisplayList.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

using namespace ScratchPadd::Scene;
using namespace ScratchPadd::Scene::DisplayList;

namespace {

// How many parameter words each command used takes
int parameterCount(std::uint8_t command) {
  switch (command) {
  case kVertex16:
    return 2;
  case kColour:
  case kNormal:
  case kDiffuseAmbient:
  case kSpecularEmission:
  case kBegin:
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
  EXPECT_EQ(sent[0].command, kBegin);
  EXPECT_EQ(sent[0].parameters[0], 1u); // quads
  EXPECT_EQ(count(sent, kBegin), 1);
  EXPECT_EQ(count(sent, kVertex16), 24);
  EXPECT_EQ(count(sent, kColour), 1); // one colour, sent once
  EXPECT_EQ(count(sent, kNormal), 0);
  EXPECT_EQ(sent[1].parameters[0], rgb15({255, 0, 0}));
  for (const Sent &s : sent) {
    if (s.command != kVertex16)
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
  EXPECT_EQ(count(sent, kVertex16), 8); // two quads
  EXPECT_EQ(count(sent, kColour), 0);
  EXPECT_EQ(count(sent, kDiffuseAmbient), 2);
  EXPECT_EQ(count(sent, kSpecularEmission), 2);
  EXPECT_EQ(count(sent, kNormal), 2); // one a quad: they face the same way, but the colour changed

  // In order: the material, its emission, the normal, then the vertices
  ASSERT_GE(sent.size(), 4u);
  EXPECT_EQ(sent[1].command, kDiffuseAmbient);
  EXPECT_EQ(sent[1].parameters[0], rgb15({200, 100, 0}));
  EXPECT_EQ(sent[2].command, kSpecularEmission);
  EXPECT_EQ(sent[2].parameters[0] >> 16, rgb15(multiply({200, 100, 0}, {128, 128, 128})));
  EXPECT_EQ(sent[3].command, kNormal);
  EXPECT_EQ(sent[3].parameters[0], packNormal({0, 0, 1}));
  EXPECT_EQ(sent[4].command, kVertex16);
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
  EXPECT_EQ(list[1], kBegin | (kColour << 8) | (kVertex16 << 16) | (kVertex16 << 24));
  EXPECT_EQ(list[8], static_cast<std::uint32_t>(kVertex16));
}

// A cooked mesh's runs are kept as they are: each starts its kind of
// polygon (a strip here), its corners in order
TEST(DisplayListTest, CookedRunsAreDrawnAsGiven) {
  const Vertex vertices[] = {{{0, 0, 0}, {0, 0, 1}, {}},
                             {{1, 0, 0}, {0, 0, 1}, {}},
                             {{0, 1, 0}, {0, 0, 1}, {}},
                             {{1, 1, 0}, {0, 0, 1}, {}}};
  const std::uint32_t indices[] = {0, 1, 2, 3};
  const Primitive runs[] = {{Primitive::Kind::triangleStrip, 31, 4}};
  const Source source = prepare({vertices, indices, runs});
  ASSERT_EQ(source.runs.size(), 1u);
  EXPECT_EQ(source.runs[0].kind, Primitive::Kind::triangleStrip);
  const std::vector<Sent> sent = unpack(onlyList(pack(source, {.lit = false})));
  EXPECT_EQ(sent[0].parameters[0], 2u); // a triangle strip
  EXPECT_EQ(count(sent, kVertex16), 4);
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
    EXPECT_EQ(count(unpack(list.words), kVertex16), 4);
}
