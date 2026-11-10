// GlslToSpirv.hpp - GLSL compiled to SPIR-V with glslang, for the desktop
// backends that take SPIR-V: Vulkan runs it, and Metal has SPIRV-Cross
// write it in Metal's language. Plain C++, tested on its own.
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ScratchPadd::Scene::Spirv {

enum class Stage { vertex, fragment };

// How the GLSL is written:
// - vulkan: for Vulkan (#version 450, its uniforms in blocks with sets and
//   bindings): the backends' own shaders
// - opengl: an OpenGL 3.30 shader (Desktop::ShaderPanel's), compiled under
//   Vulkan's rules relaxed: its loose uniforms (iTime and so on) are
//   gathered into one block, and what has no location or binding is given
//   one
enum class Dialect { vulkan, opengl };

struct Shader {
  std::vector<std::uint32_t> words; // the SPIR-V
  // An OpenGL shader's loose uniforms, gathered into one block: its set and
  // binding, its size, and each uniform's offset in it, by name
  struct Uniforms {
    std::uint32_t set = 0, binding = 0;
    std::size_t size = 0;
    std::map<std::string, std::size_t> offsets;
  };
  std::optional<Uniforms> uniforms;
};

// `glsl` as SPIR-V, or false with `error` saying why it couldn't be. An
// OpenGL shader's loose uniforms go in set 0 at `uniformBinding` (Vulkan
// gives a pipeline's two stages different ones).
bool compile(Stage stage, Dialect dialect, const std::string &glsl, Shader &out, std::string &error,
             std::uint32_t uniformBinding = 0);

} // namespace ScratchPadd::Scene::Spirv
