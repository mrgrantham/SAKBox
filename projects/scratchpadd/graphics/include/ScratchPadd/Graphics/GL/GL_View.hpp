#pragma once
#if defined(__APPLE__)
#import <CoreFoundation/CoreFoundation.h>
#endif
#include <chrono>
#include <filesystem>

#include "GL_VertexIndexBuffer.hpp"
#include <ScratchPadd/Graphics/SampleItems.hpp>

#include <DataDepRetriever/DataDependencies.hpp>

const std::string VEXTEX_SHADER_EXTENSION = "vert";
const std::string FRAGMENT_SHADER_EXTENSION = "frag";

static std::string GetShaderPath(
    const std::string &name, const std::string &extension,
    const std::string &dirPath = "/projects/scratchpadd/graphics/include/"
                                 "ScratchPadd/Graphics/GL/Shaders/") {

  // #if defined(__APPLE__)

  //   CFBundleRef mainBundle;

  //   // Get the main bundle for the app
  //   mainBundle = CFBundleGetMainBundle();
  //   CFURLRef shaderURL;
  //   // Look for a resource in the main bundle by name and type.
  //   CFStringRef shaderNameCFStr = CFStringCreateWithCString(
  //       NULL, shaderName.c_str(), kCFStringEncodingUTF8);

  //   shaderURL = CFBundleCopyResourceURL(mainBundle, shaderNameCFStr,
  //                                       CFSTR("shader"), NULL);

  //   if (shaderURL == NULL) {
  //     for (auto const& dir_entry :
  //     boost::filesystem::recursive_directory_iterator("."))
  //     {
  //         std::cout << dir_entry << '\n';
  //     }
  //     spdlog::error("Shader file not found");
  //     exit(1);
  //   }
  //   CFStringRef shaderFilePathStringRef;
  //   shaderFilePathStringRef = CFURLGetString(shaderURL);

  //   const char *shaderPath =
  //       CFStringGetCStringPtr(shaderFilePathStringRef,
  //       kCFStringEncodingUTF8);
  //   std::string shaderPathString(shaderPath);
  //   std::string URLPrefix = "file://";

  //   std::string::size_type prefixIndex = shaderPathString.find(URLPrefix);

  //   if (prefixIndex != std::string::npos) {
  //     spdlog::info("Found the file prefix");
  //     shaderPathString.erase(prefixIndex, URLPrefix.length());
  //   } else {
  //     spdlog::error("File prefix not found");
  //   }
  // #else

  // Workspace name is determined by the root name given in the WORKSPACE file
  // workspace(name = "monograntham")
  // if this changes then the workspace name needs to change too
  std::string workspace = "monograntham";

  std::string relativePath = workspace + dirPath + name + "." + extension;
  auto shaderPathString =
      DataDepRetriever::GetFullDependencyPath(relativePath);
  // #endif
  if (!shaderPathString) {
    spdlog::error("Shader file not found at : {}", relativePath);
    exit(1);
  }
  return shaderPathString.value();
}

static std::string GetVertexShaderPath(
    const std::string &name,
    const std::string &dirPath = "/projects/scratchpadd/graphics/include/"
                                 "ScratchPadd/Graphics/GL/Shaders/") {
  return GetShaderPath(name, VEXTEX_SHADER_EXTENSION, dirPath);
}

static std::string GetFragmentShaderPath(
    const std::string &name,
    const std::string &dirPath = "/projects/scratchpadd/graphics/include/"
                                 "ScratchPadd/Graphics/GL/Shaders/") {
  return GetShaderPath(name, FRAGMENT_SHADER_EXTENSION, dirPath);
}

class GL_ViewBuilder;

class GL_View : public Graphics::View {
  friend class GL_ViewBuilder;

private:
  std::unique_ptr<Graphics::FrameBuffer> frameBuffer_;
  std::unique_ptr<Graphics::VertexIndexBuffer> vertexIndexBuffer_;
  GL_Shader shader_;
  ImVec2 size_{200, 300};
  ImVec2 pos_{200, 300};
  std::string name_{"Unnamed"};
  bool open_{true};
  std::chrono::time_point<std::chrono::system_clock> start_;
  GLFWwindow *window_{nullptr};

public:
  GL_View()
      : frameBuffer_(std::make_unique<GL_FrameBuffer>()),
        vertexIndexBuffer_(std::make_unique<GL_VertexIndexBuffer>()) {
    start_ = std::chrono::system_clock::now();
  }

  void setWindow(GLFWwindow *window) override { window_ = window; }

  void setup(const std::string &&name) override {
    spdlog::info("setting up the view");
    name_ = name;
    frameBuffer_->create(800, 600);
    vertexIndexBuffer_->create(SampleItems::vertices, SampleItems::indices);
    shader_.generate(GetVertexShaderPath("example"),
                     GetFragmentShaderPath("example"));
  }

  void setBackgroundColor(ImVec4 &backgroundColor) override {
    frameBuffer_->setBackgroundColor(backgroundColor);
  }

  ImVec4 *getBackgroundColor() override {
    return frameBuffer_->getBackgroundColor();
  }

  float getSecondsSinceStart() {
    auto now = std::chrono::system_clock::now();
    // Get the time since epoch in seconds as a double
    const std::chrono::duration<float> elapsed = now - start_;

    double seconds_since_epoch = elapsed.count();

    return seconds_since_epoch;
  }

  void draw() override {

    // ImGui::SetNextWindowPos(pos_, ImGuiCond_Once);
    // ImGui::SetNextWindowSize(size_, ImGuiCond_Once);
    ImGui::SetNextWindowPos(pos_, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size_, ImGuiCond_Always);
    // ImGui::SetNextWindowContentSize(size_);
    // These vars ensure that the padding is removed so that mouse coords can be
    // calculated correctly
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
    ImGui::Begin(name_.c_str(), &open_);

    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

    shader_.use();

    // light_->update(shader_.get());

    frameBuffer_->bind();

    auto [width, height] = frameBuffer_->getSize();

    // This grabs the actual value and should be equal to the size used to set
    // the framebuffer GLint m_viewport[4]; glGetIntegerv( GL_VIEWPORT,
    // m_viewport ); ImVec2
    // iResolution(static_cast<float>(m_viewport[2]),static_cast<float>(m_viewport[3]));

    ImVec2 iResolution(static_cast<float>(width), static_cast<float>(height));
    shader_.setVec2(iResolution, "iResolution");
    shader_.setVec4(shapeColor_, "uniform_color");

    int wWidth, wHeight;
    glfwGetWindowSize(window_, &wWidth, &wHeight);
    // spdlog::info("Window w: {} h: {}", wWidth, wHeight);
    double cursorPosX, cursorPosY;
    glfwGetCursorPos(window_, &cursorPosX, &cursorPosY);
    // spdlog::info("glfwGetCursorPos x: {} y: {}", cursorPosX, cursorPosY);
    ImVec2 mousePos = ImGui::GetMousePos();
    // spdlog::info("GetMousePos x: {} y: {}", mousePos.x, mousePos.y);

    ImVec2 vMax = ImGui::GetWindowContentRegionMax();
    // spdlog::info("content region max x: {} y: {}", vMax.x, vMax.y);

    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    // spdlog::info("content region min x: {} y: {}", vMin.x, vMin.y);
    // ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    ImVec2 windowPos(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);
    // spdlog::info("GetWindowPos windowPos.x: {} windowPos.y: {}", windowPos.x,
    // windowPos.y);

    ImVec2 adjusted(vMin.x + windowPos.x, vMin.y + windowPos.y);
    // spdlog::info("content region min adjusted adjusted.x: {} adjusted.y: {}",
    // adjusted.x, adjusted.y);

    float buttonState =
        glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    float yRatio = vMax.y / viewportPanelSize.y;

    float contentAdjustedY =
        (viewportPanelSize.y - (cursorPosY - adjusted.y)) * yRatio;

    ImVec4 iMouse(static_cast<float>(cursorPosX - adjusted.x), contentAdjustedY,
                  buttonState, 0.0f);
    // spdlog::info("iMouse x: {} y: {} z: {} w: {}", iMouse.x,
    // iMouse.y,iMouse.z,iMouse.w); spdlog::info("ViewPort Panel Size x: {} y:
    // {}", viewportPanelSize.x, viewportPanelSize.y);

    // Getting imouse calculations correct is a bit cumbersome.
    // Currently window padding is off in imgui to avoid additional
    // calculations.

    // Also TODO: translate between the framebufffer size and the content area.
    // Currently they need to be the same size to get iMouse to work correctly
    shader_.setVec4(iMouse, "iMouse");

    float secondsSinceEpoch = getSecondsSinceStart();
    shader_.setFloat(secondsSinceEpoch, "iTime");

    shader_.update();
    vertexIndexBuffer_->draw();

    frameBuffer_->unbind();

    // mCamera->set_aspect(mSize.x / mSize.y);
    // mCamera->update(mShader.get());

    // add rendered texture to ImGUI scene window
    uint64_t textureID = frameBuffer_->get();
    ImGui::Image(reinterpret_cast<void *>(textureID),
                 ImVec2{viewportPanelSize.x, viewportPanelSize.y}, ImVec2{0, 1},
                 ImVec2{1, 0});

    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleVar();
  }

  void destroy() override { vertexIndexBuffer_->destroy(); }
  virtual void reset() override {}
};

// This will set all the configuration info before generating
// the GL_View object
class GL_ViewBuilder {
private:
  std::string name_ = "Not Set";
  int frameBufferX_ = 0;
  int frameBufferY_ = 0;
  int viewWidth_ = 0;
  int viewHeight_ = 0;
  int posX_ = 0;
  int posY_ = 0;
  std::string vertexShaderPath_;
  std::string fragmentShaderPath_;
  std::vector<uint32_t> indices_;
  std::vector<Graphics::VertexPair> vertices_;

public:
  // Build commands
  GL_ViewBuilder &setName(const std::string &name) {
    name_ = name;
    return *this;
  }

  GL_ViewBuilder &setFrameBuffer(int x, int y) {
    frameBufferX_ = x;
    frameBufferY_ = y;
    return *this;
  }
  GL_ViewBuilder &setSize(int width, int height) {
    viewWidth_ = width;
    viewHeight_ = height;
    return *this;
  }
  GL_ViewBuilder &setPosition(int x, int y) {
    posX_ = x;
    posY_ = y;
    return *this;
  }
  GL_ViewBuilder &setVertexShaderPath(const std::string &vertexShaderPath) {
    vertexShaderPath_ = vertexShaderPath;
    return *this;
  }
  GL_ViewBuilder &setFragmentShaderPath(const std::string &fragmentShaderPath) {
    fragmentShaderPath_ = fragmentShaderPath;
    return *this;
  }

  GL_ViewBuilder &setGeometry(const std::vector<Graphics::VertexPair> &vertices,
                              const std::vector<uint32_t> &indices) {
    vertices_ = vertices;
    indices_ = indices;
    return *this;
  }

  std::unique_ptr<Graphics::View> build() {
    auto view = std::make_unique<GL_View>();
    view->name_ = name_;
    view->frameBuffer_->create(frameBufferX_, frameBufferY_);
    // view->vertexIndexBuffer_->create(SampleItems::vertices,
    //                                  SampleItems::indices);
    // SampleItems::Circle circle(0.0, 0.0, 0.8, 100);
    // view->vertexIndexBuffer_->create(circle.vertices(), circle.indices());
    view->vertexIndexBuffer_->create(vertices_, indices_);
    view->shader_.generate(std::move(vertexShaderPath_),
                           std::move(fragmentShaderPath_));
    view->pos_.x = posX_;
    view->pos_.y = posY_;
    view->size_.x = viewWidth_;
    view->size_.y = viewHeight_;
    return view;
  }
};

// static std::unique_ptr<Graphics::View> ViewBuilder() {
//   return std::make_unique<GL_View>();
// }

static GL_ViewBuilder GetGL_ViewBuilder() {
  GL_ViewBuilder viewBuilder;
  return viewBuilder;
}