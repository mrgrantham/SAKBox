// GlslToMsl.hpp - a GLSL 3.30 shader in Metal's shading language: compiled
// to SPIR-V as an OpenGL shader (../../spirv: loose uniforms gathered into
// one block, locations and bindings given where it has none), which
// SPIRV-Cross writes as Metal.
// For Desktop::ShaderPanel; plain C++, tested on its own.
#pragma once

#include <cstddef>
#include <map>
#include <string>

namespace ScratchPadd::Scene::Metal {

struct TranslatedShader {
  std::string source;     // Metal shading language
  std::string entryPoint; // its function's name
  // The shader's loose uniforms, gathered into one buffer: its index in
  // the shader, its size, and each uniform's offset in it, by name (no
  // buffer: -1)
  int uniformBuffer = -1;
  std::size_t uniformSize = 0;
  std::map<std::string, std::size_t> uniformOffsets;
};

enum class Stage { vertex, fragment };

// `glsl` as Metal, or "" in `error` saying why it couldn't be. The vertex
// stage's y is flipped, so a texture drawn into is laid out as OpenGL lays
// it out (row 0 at the bottom of clip space), and gl_FragCoord counts from
// the same row as it does there.
bool translate(Stage stage, const std::string &glsl, TranslatedShader &out, std::string &error);

} // namespace ScratchPadd::Scene::Metal
