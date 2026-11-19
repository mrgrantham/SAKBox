
// shrunk: a picture made smaller keeps its colours where it's
// one colour, its see-through parts see-through, and a half-covered edge
// keeps the opaque colour (not darkened toward the see-through pixels')
TEST(RecipePixelsTest, ShrunkAveragesByOpacity) {
  using ScratchPadd::Scene::Colour;
  const int from = 35, to = 32;
  std::vector<Colour> pixels(static_cast<std::size_t>(from * from), Colour{0, 0, 0, 0});
  for (int y = 0; y < from; ++y)
    for (int x = 0; x < from / 2; ++x)
      pixels[static_cast<std::size_t>(y * from + x)] = {200, 100, 50, 255};
  const auto small = shrunk(pixels, from, to);
  ASSERT_EQ(small.size(), static_cast<std::size_t>(to * to));
  EXPECT_EQ(small[0], (Colour{200, 100, 50, 255}));
  EXPECT_EQ(small[static_cast<std::size_t>(to - 1)].a, 0);
  for (const Colour &c : small)
    if (c.a > 0) {
      EXPECT_EQ(c.r, 200);
      EXPECT_EQ(c.g, 100);
    }
  EXPECT_TRUE(shrunk(pixels, from, 40).empty()); // (not bigger)
}
