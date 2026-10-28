#pragma once

// ScratchPadd's look for Dear ImGui: a mid-century palette (walnut, cream,
// teal, orange and mustard), roomy spacing with soft corners, Futura for
// window titles and Questrial, a geometric sans in the same spirit, for
// everything else. Every ScratchPadd window
// calls Theme::apply() once (DebugUi does); see the README's Debug UI section.

#include "imgui.h"

namespace Theme {

enum class Colors {
  Dark,  // walnut and charcoal, cream text, teal title bars
  Light, // cream and sand, dark text, pale teal title bars
};

// Sets the current ImGui context's colors, spacing and fonts. Call once,
// after ImGui::CreateContext() and before the first frame.
//
// Fonts: titles are Futura Medium where the system has it (macOS's, at
// /System/Library/Fonts/Supplemental/Futura.ttc; Futura can't be shipped
// with the program), otherwise Jost Medium, a free font in Futura's spirit.
// Text is Questrial. Jost and Questrial are compiled into the program, so
// there's no font file to find when it runs.
void apply(Colors colors = Colors::Dark);

// The font for titles and headings (Futura or Jost Medium; ImGui's default
// font before apply())
ImFont *titleFont();

// ImGui::Begin(), with the window's title in the title font. Call
// ImGui::End() afterwards whatever it returns, as for ImGui::Begin().
bool beginWindow(const char *name, bool *open = nullptr, ImGuiWindowFlags flags = 0);

// Text in the title font, for headings inside a window
void heading(const char *text);

} // namespace Theme
