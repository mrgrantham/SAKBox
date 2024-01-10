#pragma once
#if defined(__APPLE__)
#import <CoreFoundation/CoreFoundation.h>
#endif
#include <filesystem>

#include "GL_SampleItems.hpp"
#include "GL_VertexIndexBuffer.hpp"

#include <ScratchPadd/DataDependencies.hpp>

static std::string getShaderPath(const std::string &&shaderName) {

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

  auto shaderPathString = ScratchPadd::Data::GetFullDependencyPath(
      "graphFun/projects/scratchpadd/graphics/include/ScratchPadd/Graphics/GL/"
      "Shaders/" +
      shaderName + ".shader");
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
  ImVec2 size_;
  std::string name_{"Unnamed"};

public:
  GL_View()
      : frameBuffer_(std::make_unique<GL_FrameBuffer>()),
        vertexIndexBuffer_(std::make_unique<GL_VertexIndexBuffer>()) {}

  void setup(const std::string &&name) override {
    spdlog::info("setting up the view");
    name_ = name;
    frameBuffer_->create(800, 600);
    vertexIndexBuffer_->create(GL_SampleItems::vertices,
                               GL_SampleItems::indices);
    shader_.generate(getShaderPath("vertex"), getShaderPath("fragment"));
  }

  void setBackgroundColor(ImVec4 &backgroundColor) override {
    frameBuffer_->setBackgroundColor(backgroundColor);
  }

  ImVec4 *getBackgroundColor() override {
    return frameBuffer_->getBackgroundColor();
  }

  void draw() override {
    shader_.use();
    // light_->update(shader_.get());

    frameBuffer_->bind();

    shader_.setVec4(shapeColor_, "uniform_color");
    shader_.update();
    vertexIndexBuffer_->draw();

    frameBuffer_->unbind();

    ImGui::Begin(name_.c_str());

    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    size_ = {viewportPanelSize.x, viewportPanelSize.y};

    // mCamera->set_aspect(mSize.x / mSize.y);
    // mCamera->update(mShader.get());

    // add rendered texture to ImGUI scene window
    uint64_t textureID = frameBuffer_->get();
    ImGui::Image(reinterpret_cast<void *>(textureID), ImVec2{size_.x, size_.y},
                 ImVec2{0, 1}, ImVec2{1, 0});

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
  std::string vertexShaderPath_;
  std::string fragmentShaderPath_;

public:
  // Build commands
  GL_ViewBuilder &setName(const std::string &name) {
    spdlog::info("setting name");
    name_ = name;
    return *this;
  }

  GL_ViewBuilder &setFrameBuffer(int x, int y) {
    spdlog::info("setting fb");
    frameBufferX_ = x;
    frameBufferY_ = y;
    spdlog::info("done setting fb");
    return *this;
  }
  GL_ViewBuilder &setVertexShaderPath(const std::string &vertexShaderPath) {
    spdlog::info("setting shader v");
    vertexShaderPath_ = vertexShaderPath;
    return *this;
  }
  GL_ViewBuilder &setFragmentShaderPath(const std::string &fragmentShaderPath) {
    spdlog::info("setting shader f");
    fragmentShaderPath_ = fragmentShaderPath;
    return *this;
  }

  std::unique_ptr<Graphics::View> build() {
    spdlog::info("building the view");
    auto view = std::make_unique<GL_View>();
    view->name_ = name_;
    view->frameBuffer_->create(frameBufferX_, frameBufferY_);
    view->vertexIndexBuffer_->create(GL_SampleItems::vertices,
                                     GL_SampleItems::indices);
    spdlog::info("Fragment shader: {} Vertex shader: {}", fragmentShaderPath_,
                 vertexShaderPath_);
    view->shader_.generate(std::move(vertexShaderPath_),
                           std::move(fragmentShaderPath_));
    spdlog::info("built the view");
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