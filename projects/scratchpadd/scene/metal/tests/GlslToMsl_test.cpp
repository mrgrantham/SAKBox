// GLSL 3.30 shaders in Metal's shading language: every shader of the shader
// playground translates, and Metal compiles what it becomes
#include <ExpectedCapabilities/ExpectedCapabilities.hpp>
#include <GlslToMsl.hpp>
#include <ScratchPadd/Scene/Metal/Metal.hpp>

#include <Metal/Metal.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ScratchPadd::Scene::Metal;

namespace {

std::string read(const std::filesystem::path &path) {
  std::ifstream file(path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

// Whether Metal compiles `source`; why not, if it doesn't
bool compiles(const std::string &source, std::string &why) {
  NS::Error *error = nullptr;
  MTL::Library *library = device()->newLibrary(
      NS::String::string(source.c_str(), NS::UTF8StringEncoding), nullptr, &error);
  if (!library) {
    why = error ? error->localizedDescription()->utf8String() : "no library";
    return false;
  }
  library->release();
  return true;
}

} // namespace

TEST(GlslToMslTest, EveryPlaygroundShaderTranslatesAndCompiles) {
  if (!device())
    SKIP_OR_FAIL_WITHOUT("metal", "no Metal device");
  const std::filesystem::path shaders = std::filesystem::path(std::getenv("TEST_SRCDIR")) /
                                        "_main/projects/examples/shader_playground/shaders";
  int translated = 0;
  for (const auto &entry : std::filesystem::directory_iterator(shaders)) {
    const std::string extension = entry.path().extension().string();
    if (extension != ".vert" && extension != ".frag")
      continue;
    SCOPED_TRACE(entry.path().filename().string());
    TranslatedShader shader;
    std::string why;
    ASSERT_TRUE(translate(extension == ".vert" ? Stage::vertex : Stage::fragment,
                          read(entry.path()), shader, why))
        << why;
    EXPECT_FALSE(shader.entryPoint.empty());
    EXPECT_TRUE(compiles(shader.source, why)) << why << "\n" << shader.source;
    ++translated;
  }
  EXPECT_EQ(translated, 18);
}

// Loose uniforms are gathered into one buffer, each at an offset Metal's
// layout gives it; a mistake in GLSL is reported, not translated
TEST(GlslToMslTest, UniformsAreFoundAndErrorsReported) {
  TranslatedShader shader;
  std::string why;
  ASSERT_TRUE(translate(Stage::fragment, R"(#version 330 core
layout(location = 0) out vec4 color;
uniform float iTime;
uniform vec2 iResolution;
uniform vec4 uniform_color;
void main() { color = uniform_color * iTime + vec4(gl_FragCoord.xy / iResolution, 0.0, 1.0); }
)",
                        shader, why))
      << why;
  EXPECT_GE(shader.uniformBuffer, 0);
  ASSERT_EQ(shader.uniformOffsets.size(), 3u);
  EXPECT_EQ(shader.uniformOffsets.at("iTime"), 0u);
  EXPECT_EQ(shader.uniformOffsets.at("iResolution"), 8u); // a vec2's alignment
  EXPECT_EQ(shader.uniformOffsets.at("uniform_color"), 16u);
  EXPECT_EQ(shader.uniformSize, 32u);

  EXPECT_FALSE(
      translate(Stage::fragment, "#version 330 core\nvoid main() { nonsense; }", shader, why));
  EXPECT_NE(why.find("nonsense"), std::string::npos) << why;
}
