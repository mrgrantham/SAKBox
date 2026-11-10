// GLSL to SPIR-V: every shader of the shader playground (OpenGL 3.30)
// compiles, Vulkan GLSL compiles, a shader's loose uniforms are found, and a
// mistake is reported
#include <ScratchPadd/Scene/Spirv/GlslToSpirv.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ScratchPadd::Scene::Spirv;

namespace {

std::string read(const std::filesystem::path &path) {
  std::ifstream file(path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

constexpr std::uint32_t kSpirvMagic = 0x07230203;

} // namespace

TEST(GlslToSpirvTest, EveryPlaygroundShaderCompiles) {
  const std::filesystem::path shaders = std::filesystem::path(std::getenv("TEST_SRCDIR")) /
                                        "_main/projects/examples/shader_playground/shaders";
  int compiled = 0;
  for (const auto &entry : std::filesystem::directory_iterator(shaders)) {
    const std::string extension = entry.path().extension().string();
    if (extension != ".vert" && extension != ".frag")
      continue;
    SCOPED_TRACE(entry.path().filename().string());
    Shader shader;
    std::string why;
    ASSERT_TRUE(compile(extension == ".vert" ? Stage::vertex : Stage::fragment, Dialect::opengl,
                        read(entry.path()), shader, why))
        << why;
    ASSERT_FALSE(shader.words.empty());
    EXPECT_EQ(shader.words[0], kSpirvMagic);
    ++compiled;
  }
  EXPECT_EQ(compiled, 18);
}

// Loose uniforms are gathered into one block, each at std140's offset
TEST(GlslToSpirvTest, LooseUniformsAreFound) {
  Shader shader;
  std::string why;
  ASSERT_TRUE(compile(Stage::fragment, Dialect::opengl, R"(#version 330 core
layout(location = 0) out vec4 color;
uniform float iTime;
uniform vec2 iResolution;
uniform vec4 uniform_color;
void main() { color = uniform_color * iTime + vec4(gl_FragCoord.xy / iResolution, 0.0, 1.0); }
)",
                      shader, why))
      << why;
  ASSERT_TRUE(shader.uniforms);
  const Shader::Uniforms uniforms = shader.uniforms.value_or(Shader::Uniforms{});
  ASSERT_EQ(uniforms.offsets.size(), 3u);
  EXPECT_EQ(uniforms.offsets.at("iTime"), 0u);
  EXPECT_EQ(uniforms.offsets.at("iResolution"), 8u); // a vec2's alignment
  EXPECT_EQ(uniforms.offsets.at("uniform_color"), 16u);
  EXPECT_EQ(uniforms.size, 32u);
  EXPECT_EQ(uniforms.set, 0u);
  EXPECT_EQ(uniforms.binding, 0u);

  // Put where it's asked to be
  ASSERT_TRUE(compile(Stage::vertex, Dialect::opengl, R"(#version 330 core
layout(location = 0) in vec3 position;
uniform float iTime;
void main() { gl_Position = vec4(position * iTime, 1.0); }
)",
                      shader, why, 3))
      << why;
  ASSERT_TRUE(shader.uniforms);
  const Shader::Uniforms placed = shader.uniforms.value_or(Shader::Uniforms{});
  EXPECT_EQ(placed.binding, 3u);
}

// Vulkan GLSL, as the backends' own shaders are written, compiles as it is;
// a mistake in either is reported, not compiled
TEST(GlslToSpirvTest, VulkanGlslCompilesAndMistakesAreReported) {
  Shader shader;
  std::string why;
  EXPECT_TRUE(compile(Stage::vertex, Dialect::vulkan, R"(#version 450
layout(set = 0, binding = 0) uniform Frame { mat4 viewProj; } frame;
layout(location = 0) in vec3 position;
void main() { gl_Position = frame.viewProj * vec4(position, 1.0); }
)",
                      shader, why))
      << why;
  EXPECT_FALSE(shader.uniforms);

  EXPECT_FALSE(compile(Stage::fragment, Dialect::opengl,
                       "#version 330 core\nvoid main() { nonsense; }", shader, why));
  EXPECT_NE(why.find("nonsense"), std::string::npos) << why;
  // Vulkan's rules, not relaxed: a loose uniform isn't allowed
  EXPECT_FALSE(compile(Stage::fragment, Dialect::vulkan,
                       "#version 450\nuniform float t;\nvoid main() {}", shader, why));
}
