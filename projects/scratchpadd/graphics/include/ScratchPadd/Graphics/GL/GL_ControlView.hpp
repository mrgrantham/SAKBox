#pragma once

class GL_ControlView : public Graphics::ControlView {

public:
  void setWindow(GLFWwindow *window) override {}
  void draw() override {
    ImGui::SetNextWindowSize({300, 150}, ImGuiCond_FirstUseEver);
    ImGui::Begin(controls_.paddName.c_str());
    buildControls(controls_);
    ImGui::End();
  }

  void destroy() override {}

  void reset() override {}

  void setup(const std::string &&name) override {}
};

static std::unique_ptr<Graphics::ControlView> ControlViewBuilder() {
  return std::make_unique<GL_ControlView>();
}