#pragma once

class GL_ControlView : public Graphics::ControlView {

public:
  void setWindow(GLFWwindow *window) override {}
  void draw() override {
    if (placement_) {
      // Keep a 10 px margin from the window edge the panel is placed against
      const ImVec2 display = ImGui::GetIO().DisplaySize;
      const ImVec2 position(10 + placement_->x * (display.x - 20),
                            10 + placement_->y * (display.y - 20));
      ImGui::SetNextWindowPos(position, ImGuiCond_FirstUseEver, *placement_);
    }
    // Sized to fit its controls, so labels aren't cut off and nothing scrolls
    ImGui::Begin(controls_.paddName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    buildControls();
    ImGui::End();
  }

  void destroy() override {}

  void reset() override {}

  void setup(const std::string &&name) override {}
};

static std::unique_ptr<Graphics::ControlView> ControlViewBuilder() {
  return std::make_unique<GL_ControlView>();
}