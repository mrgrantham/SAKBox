// Gltf.hpp - a model's mesh read from the glTF Blender exported (.glb)
#pragma once

#include "Cook.hpp"

#include <optional>
#include <string>

namespace Cook {

struct Model {
  Mesh mesh;
  std::string object; // the object it came from (Ship, or Ship_DS)
  // Whether every material on it has Blender's Backface Culling ticked
  // (glTF's doubleSided false, set explicitly), which culls it however
  // it's shaped
  bool backfaceCulling = false;
};

// The version of `object` for `platform` ("desktop", "ds") in the glTF at
// `path`: an object of its own for the platform if there is one (named
// `object` + "_" + the platform in capitals, as Ship_DS, or with the custom
// property sak.platform set to the platform and a name starting `object`),
// else `object` itself. Its mesh in its own coordinates (where it sits in
// the .blend doesn't matter), its vertex colours as sRGB (Blender exports
// them linear), white where it has none. nullopt, with `error` saying why,
// if there's no such object or it can't be read.
std::optional<Model> readModel(const std::string &path, const std::string &object,
                               const std::string &platform, std::string &error);

} // namespace Cook
