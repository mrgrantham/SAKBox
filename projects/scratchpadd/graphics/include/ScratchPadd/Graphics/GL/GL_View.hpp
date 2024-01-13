#pragma once
#if defined(__APPLE__)
#import <CoreFoundation/CoreFoundation.h>
#endif
#include <chrono>
#include <filesystem>

#include "GL_VertexIndexBuffer.hpp"
#include <ScratchPadd/Graphics/SampleItems.hpp>

#include <ScratchPadd/DataDependencies.hpp>

static std::string getShaderPath(const std::string &shaderName,
                                 const std::string &shaderDirPath =
                                     "/projects/scratchpadd/graphics/include/"
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

  auto shaderPathString = ScratchPadd::Data::GetFullDependencyPath(
      workspace + shaderDirPath + shaderName + ".shader");
  // #endif
  if (!shaderPathString) {
    spdlog::error("Shader file not found");
    exit(1);
  }
  return shaderPathString.value();
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

public:
  GL_View()
      : frameBuffer_(std::make_unique<GL_FrameBuffer>()),
        vertexIndexBuffer_(std::make_unique<GL_VertexIndexBuffer>()) {
    start_ = std::chrono::system_clock::now();
  }

  void setup(const std::string &&name) override {
    spdlog::info("setting up the view");
    name_ = name;
    frameBuffer_->create(800, 600);
    vertexIndexBuffer_->create(SampleItems::vertices, SampleItems::indices);
    shader_.generate(getShaderPath("vertex"), getShaderPath("fragment"));
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

    float secondsSinceEpoch = getSecondsSinceStart();
    shader_.setFloat(secondsSinceEpoch, "time");

    shader_.update();
    vertexIndexBuffer_->draw();

    frameBuffer_->unbind();

    ImGui::SetNextWindowPos(pos_, ImGuiCond_Once);
    ImGui::SetNextWindowSize(size_, ImGuiCond_Once);
    ImGui::Begin(name_.c_str(), &open_);

    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    // size_ = {viewportPanelSize.x, viewportPanelSize.y};

    // mCamera->set_aspect(mSize.x / mSize.y);
    // mCamera->update(mShader.get());

    // add rendered texture to ImGUI scene window
    uint64_t textureID = frameBuffer_->get();
    ImGui::Image(reinterpret_cast<void *>(textureID),
                 ImVec2{viewportPanelSize.x, viewportPanelSize.y}, ImVec2{0, 1},
                 ImVec2{1, 0});

    ImGui::End();
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