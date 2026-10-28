// ScratchPadd's ImGui theme (Theme.hpp), applied to an ImGui context with no
// window: the compiled-in fonts are real fonts, and the colors, spacing and
// fonts are set.
#include <ScratchPadd/DebugUi/Theme.hpp>

#include <gtest/gtest.h>

#include <cstddef>

namespace Theme::Fonts {
extern const unsigned char jostMedium[];
extern const std::size_t jostMediumSize;
extern const unsigned char questrial[];
extern const std::size_t questrialSize;
} // namespace Theme::Fonts

namespace {

// A TrueType font starts with its version, 1.0
bool isTrueType(const unsigned char *data, std::size_t size) {
  return size > 4 && data[0] == 0 && data[1] == 1 && data[2] == 0 && data[3] == 0;
}

class ThemeTest : public testing::Test {
protected:
  void SetUp() override { ImGui::CreateContext(); }
  void TearDown() override { ImGui::DestroyContext(); }
};

} // namespace

TEST(ThemeFontsTest, CompiledInFontsAreTrueType) {
  EXPECT_TRUE(isTrueType(Theme::Fonts::jostMedium, Theme::Fonts::jostMediumSize));
  EXPECT_TRUE(isTrueType(Theme::Fonts::questrial, Theme::Fonts::questrialSize));
}

// Text in Questrial, titles in a font of their own, the palette and the
// spacing
TEST_F(ThemeTest, AppliesFontsColorsAndSpacing) {
  Theme::apply(Theme::Colors::Dark);
  const ImGuiIO &io = ImGui::GetIO();
  ASSERT_NE(io.FontDefault, nullptr);
  ASSERT_NE(Theme::titleFont(), nullptr);
  EXPECT_NE(Theme::titleFont(), io.FontDefault);
  const ImGuiStyle &style = ImGui::GetStyle();
  EXPECT_EQ(style.FrameRounding, 4.0f);
  EXPECT_EQ(style.WindowPadding.x, 12.0f);
  EXPECT_GT(style.Colors[ImGuiCol_Text].x, 0.9f); // cream

  Theme::apply(Theme::Colors::Light);
  EXPECT_LT(ImGui::GetStyle().Colors[ImGuiCol_Text].x, 0.2f); // dark brown
  EXPECT_NEAR(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x, 0xF3 / 255.0f,
              1e-3f); // cream // not ImGui's own
}
