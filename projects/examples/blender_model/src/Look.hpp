// Look.hpp - the example's look, switched while it runs (the same on the
// desktop, the DS and the 3DS): smooth lighting or toon shading's bands,
// how many bands, outlines, and on the DS how its toon table is used.
#pragma once

#include <ScratchPadd/Scene/Types.hpp>

#include <cstdio>

namespace Look {

namespace Scene = ScratchPadd::Scene;

struct Settings {
  bool toon = true;
  bool outlines = true;
  int bands = 3;
  Scene::DsToonTable dsTable = Scene::DsToonTable::Shades;

  void nextBands() { bands = bands >= 5 ? 2 : bands + 1; }
  void switchDsTable() {
    dsTable = dsTable == Scene::DsToonTable::Highlights ? Scene::DsToonTable::Shades
                                                        : Scene::DsToonTable::Highlights;
  }

  // A model's material in this look (unlit ones stay unlit)
  Scene::Material material(Scene::Material material) const {
    if (material.shading != Scene::Material::Shading::Unlit)
      material.shading = toon ? Scene::Material::Shading::Toon : Scene::Material::Shading::Lit;
    material.outline = outlines;
    return material;
  }
  void apply(Scene::Environment &environment) const {
    environment.toonBands = static_cast<std::uint8_t>(bands);
    environment.dsToonTable = dsTable;
  }

  // "Toon, 3 bands, outlines", for a line of text
  void describe(char *text, std::size_t size) const {
    std::snprintf(text, size, "%s%s%s", toon ? "Toon, " : "Smooth lighting",
                  toon ? (bands == 2   ? "2 bands"
                          : bands == 3 ? "3 bands"
                          : bands == 4 ? "4 bands"
                                       : "5 bands")
                       : "",
                  outlines ? ", outlines" : "");
  }
};

} // namespace Look
