// ShaderPanels on Vulkan: every fragment shader of the shader playground,
// with its vertex shader, is compiled, made into a pipeline the device
// takes, and drawn; a mistake is
// reported. Skipped where there's no Vulkan.
#include <ScratchPadd/Scene/Desktop/Desktop.hpp>
#include <ScratchPadd/Scene/MeshData.hpp>
#include <ScratchPadd/Scene/Vulkan/Vulkan.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace ScratchPadd::Scene;

namespace {

std::string read(const std::filesystem::path &path) {
  std::ifstream file(path);
  std::stringstream text;
  text << file.rdbuf();
  return text.str();
}

// A square covering the panel, as the playground draws
MeshBuilder screen() {
  MeshBuilder builder;
  builder.quad({-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0});
  return builder;
}

} // namespace

TEST(VulkanShaderPanelTest, EveryPlaygroundShaderMakesAPanel) {
  if (!Vulkan::context())
    GTEST_SKIP() << "no Vulkan: " << Vulkan::whyNoVulkan();
  const std::filesystem::path shaders = std::filesystem::path(std::getenv("TEST_SRCDIR")) /
                                        "_main/projects/examples/shader_playground/shaders";
  const MeshBuilder mesh = screen();
  int made = 0;
  for (const auto &entry : std::filesystem::directory_iterator(shaders)) {
    if (entry.path().extension() != ".frag")
      continue;
    SCOPED_TRACE(entry.path().filename().string());
    // Its own vertex shader (gl_view.frag needs what gl_view.vert sends
    // it), or the playground's default
    std::filesystem::path vertex = entry.path();
    vertex.replace_extension(".vert");
    if (!std::filesystem::exists(vertex))
      vertex = shaders / "default.vert";
    Desktop::ShaderPanel panel(64, 64, read(vertex), read(entry.path()), mesh.data());
    EXPECT_TRUE(panel.ok()) << panel.error();
    panel.render(1.5f, 10, 20, true, Colour{200, 100, 50});
    panel.render(1.6f, 10, 20, false, Colour{200, 100, 50});
    ++made;
  }
  EXPECT_EQ(made, 12);
}

TEST(VulkanShaderPanelTest, MistakesAreReported) {
  if (!Vulkan::context())
    GTEST_SKIP() << "no Vulkan: " << Vulkan::whyNoVulkan();
  const MeshBuilder mesh = screen();
  Desktop::ShaderPanel panel(16, 16, "#version 330 core\nvoid main() {}",
                             "#version 330 core\nvoid main() { nonsense; }", mesh.data());
  EXPECT_FALSE(panel.ok());
  EXPECT_NE(panel.error().find("fragment shader"), std::string::npos) << panel.error();
  panel.render(0, 0, 0, false, Colour{}); // draws nothing, and doesn't fail
}
