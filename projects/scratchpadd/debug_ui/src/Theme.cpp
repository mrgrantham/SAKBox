#include <ScratchPadd/DebugUi/Theme.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>

// The fonts compiled in (see BUILD: embed_file)
namespace Theme::Fonts {
extern const unsigned char jostMedium[];
extern const std::size_t jostMediumSize;
extern const unsigned char questrial[];
extern const std::size_t questrialSize;
} // namespace Theme::Fonts

namespace Theme {

namespace {

// (Questrial is drawn larger than most fonts at the same size, so it needs
// less of one)
constexpr float kTextSize = 15.0f;
// Futura's lowercase letters are short, so titles need a larger size than
// the text to look larger (at 17 they looked smaller than 15 px Questrial)
constexpr float kTitleSize = 20.0f;
// macOS's own Futura; Futura Medium is the collection's first face
constexpr const char *kSystemFutura = "/System/Library/Fonts/Supplemental/Futura.ttc";
constexpr int kFuturaMediumFace = 0;

ImFont *titleFont_ = nullptr;

constexpr ImVec4 rgb(std::uint32_t hex, float alpha = 1.0f) {
  return ImVec4(static_cast<float>((hex >> 16) & 0xFF) / 255.0f,
                static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
                static_cast<float>(hex & 0xFF) / 255.0f, alpha);
}

ImVec4 withAlpha(ImVec4 color, float alpha) { return ImVec4(color.x, color.y, color.z, alpha); }

// The few colors a theme chooses; setColors() derives ImGui's ~55 from them
struct Palette {
  ImVec4 window;      // window backgrounds
  ImVec4 popup;       // popups, menus, tooltips
  ImVec4 frame;       // sliders, inputs, checkboxes, buttons at rest
  ImVec4 frameHover;  // ... under the mouse
  ImVec4 frameActive; // ... being used
  ImVec4 title;       // title bars of windows not in focus
  ImVec4 titleActive; // the title bar of the window in focus
  ImVec4 accent;      // slider grabs, check marks, pressed buttons (orange)
  ImVec4 accentHover; // ... under the mouse or dragged
  ImVec4 highlight;   // headers, selections, tabs (mustard)
  ImVec4 text;
  ImVec4 textDim;
  ImVec4 border;
};

constexpr Palette kDark{
    .window = rgb(0x231F1C),
    .popup = rgb(0x2B2622),
    .frame = rgb(0x3A322B),
    .frameHover = rgb(0x4A3F36),
    .frameActive = rgb(0x57493D),
    .title = rgb(0x1F3A3D),
    .titleActive = rgb(0x2A6F6B),
    .accent = rgb(0xD9622B),
    .accentHover = rgb(0xEC7A42),
    .highlight = rgb(0xE0A526),
    .text = rgb(0xF2E8D5),
    .textDim = rgb(0xA89C8A),
    .border = rgb(0x4A3F36),
};

constexpr Palette kLight{
    .window = rgb(0xF3EBDD),
    .popup = rgb(0xFBF6EC),
    .frame = rgb(0xE6D9C3),
    .frameHover = rgb(0xDCCBAE),
    .frameActive = rgb(0xD0BB98),
    .title = rgb(0xC9DDD9),
    .titleActive = rgb(0x8FC1B9),
    .accent = rgb(0xD9622B),
    .accentHover = rgb(0xC4521F),
    .highlight = rgb(0xE8B84A),
    .text = rgb(0x2B2622),
    .textDim = rgb(0x7A6E60),
    .border = rgb(0xD0BB98),
};

void setColors(const Palette &p) {
  ImVec4 *c = ImGui::GetStyle().Colors;
  c[ImGuiCol_Text] = p.text;
  c[ImGuiCol_TextDisabled] = p.textDim;
  c[ImGuiCol_WindowBg] = p.window; // opaque, so overlapping panels don't show through
  c[ImGuiCol_ChildBg] = withAlpha(p.window, 0.0f);
  c[ImGuiCol_PopupBg] = withAlpha(p.popup, 0.98f);
  c[ImGuiCol_Border] = p.border;
  c[ImGuiCol_BorderShadow] = withAlpha(p.border, 0.0f);
  c[ImGuiCol_FrameBg] = p.frame;
  c[ImGuiCol_FrameBgHovered] = p.frameHover;
  c[ImGuiCol_FrameBgActive] = p.frameActive;
  c[ImGuiCol_TitleBg] = p.title;
  c[ImGuiCol_TitleBgActive] = p.titleActive;
  c[ImGuiCol_TitleBgCollapsed] = p.title;
  c[ImGuiCol_MenuBarBg] = p.popup;
  c[ImGuiCol_ScrollbarBg] = withAlpha(p.window, 0.0f);
  c[ImGuiCol_ScrollbarGrab] = p.frameHover;
  c[ImGuiCol_ScrollbarGrabHovered] = p.frameActive;
  c[ImGuiCol_ScrollbarGrabActive] = p.accent;
  c[ImGuiCol_CheckMark] = p.accent;
  c[ImGuiCol_SliderGrab] = p.accent;
  c[ImGuiCol_SliderGrabActive] = p.accentHover;
  c[ImGuiCol_Button] = p.frame;
  c[ImGuiCol_ButtonHovered] = p.frameHover;
  c[ImGuiCol_ButtonActive] = p.accent;
  c[ImGuiCol_Header] = withAlpha(p.highlight, 0.22f);
  c[ImGuiCol_HeaderHovered] = withAlpha(p.highlight, 0.4f);
  c[ImGuiCol_HeaderActive] = withAlpha(p.highlight, 0.6f);
  c[ImGuiCol_Separator] = p.border;
  c[ImGuiCol_SeparatorHovered] = p.accentHover;
  c[ImGuiCol_SeparatorActive] = p.accent;
  c[ImGuiCol_ResizeGrip] = withAlpha(p.accent, 0.25f);
  c[ImGuiCol_ResizeGripHovered] = withAlpha(p.accent, 0.6f);
  c[ImGuiCol_ResizeGripActive] = p.accent;
  c[ImGuiCol_Tab] = p.frame;
  c[ImGuiCol_TabHovered] = withAlpha(p.highlight, 0.55f);
  c[ImGuiCol_TabSelected] = p.titleActive;
  c[ImGuiCol_TableHeaderBg] = p.frame;
  c[ImGuiCol_TableBorderStrong] = p.border;
  c[ImGuiCol_TableBorderLight] = withAlpha(p.border, 0.5f);
  c[ImGuiCol_TableRowBg] = withAlpha(p.frame, 0.0f);
  c[ImGuiCol_TableRowBgAlt] = withAlpha(p.frame, 0.35f);
  c[ImGuiCol_PlotHistogram] = p.highlight;
  c[ImGuiCol_PlotHistogramHovered] = p.accent;
  c[ImGuiCol_TextSelectedBg] = withAlpha(p.highlight, 0.35f);
}

// Roomy spacing and soft corners
void setSpacing() {
  ImGuiStyle &style = ImGui::GetStyle();
  style.WindowPadding = ImVec2(12, 10);
  style.FramePadding = ImVec2(8, 5);
  style.ItemSpacing = ImVec2(10, 7);
  style.ItemInnerSpacing = ImVec2(6, 5);
  style.IndentSpacing = 18;
  style.GrabMinSize = 12;
  style.WindowRounding = 6;
  style.ChildRounding = 4;
  style.FrameRounding = 4;
  style.PopupRounding = 4;
  style.ScrollbarRounding = 6;
  style.GrabRounding = 4;
  style.TabRounding = 4;
  style.WindowBorderSize = 1;
  style.FrameBorderSize = 0;
  style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
}

// One of the compiled-in fonts. The data stays ours: the atlas mustn't
// free it.
ImFont *addCompiledIn(const unsigned char *data, std::size_t size, float pixels) {
  ImFontConfig config;
  config.FontDataOwnedByAtlas = false;
  return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(data),
                                                    static_cast<int>(size), pixels, &config);
}

ImFont *addTitleFont() {
  std::error_code missing;
  if (std::filesystem::exists(kSystemFutura, missing)) {
    ImFontConfig config;
    config.FontNo = kFuturaMediumFace;
    if (ImFont *futura =
            ImGui::GetIO().Fonts->AddFontFromFileTTF(kSystemFutura, kTitleSize, &config)) {
      return futura;
    }
  }
  return addCompiledIn(Fonts::jostMedium, Fonts::jostMediumSize, kTitleSize);
}

} // namespace

void apply(Colors colors) {
  ImGui::StyleColorsDark(); // fills in what the palette leaves out
  if (colors == Colors::Light) {
    ImGui::StyleColorsLight();
  }
  setColors(colors == Colors::Dark ? kDark : kLight);
  setSpacing();
  ImGuiIO &io = ImGui::GetIO();
  io.Fonts->Clear();
  io.FontDefault = addCompiledIn(Fonts::questrial, Fonts::questrialSize, kTextSize);
  ImGui::GetStyle().FontSizeBase = kTextSize;
  titleFont_ = addTitleFont();
}

ImFont *titleFont() { return titleFont_ ? titleFont_ : ImGui::GetFont(); }

bool beginWindow(const char *name, bool *open, ImGuiWindowFlags flags) {
  ImGui::PushFont(titleFont(), kTitleSize);
  const bool shown = ImGui::Begin(name, open, flags);
  ImGui::PopFont();
  return shown;
}

void heading(const char *text) {
  ImGui::PushFont(titleFont(), kTitleSize);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
}

} // namespace Theme
