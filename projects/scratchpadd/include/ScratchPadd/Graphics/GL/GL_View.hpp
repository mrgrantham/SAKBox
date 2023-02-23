#pragma once
#if defined(__APPLE__)
#import <CoreFoundation/CoreFoundation.h>
#endif

static std::string getShaderPath(const std::string &&shaderName) {
#if defined(__APPLE__)

CFBundleRef mainBundle;
 
// Get the main bundle for the app
mainBundle = CFBundleGetMainBundle();
CFURLRef    shaderURL;
// Look for a resource in the main bundle by name and type.
CFStringRef shaderNameCFStr = CFStringCreateWithCString(NULL,shaderName.c_str(),kCFStringEncodingUTF8);

shaderURL = CFBundleCopyResourceURL( mainBundle,
                shaderNameCFStr,
                CFSTR("shader"),
                NULL );

if (shaderURL == NULL) {
  spdlog::error("Shader file not found");
  exit(1);
}
CFStringRef shaderFilePathStringRef;
shaderFilePathStringRef = CFURLGetString(shaderURL);

const char * shaderPath = CFStringGetCStringPtr(shaderFilePathStringRef,kCFStringEncodingUTF8);
std::string shaderPathString(shaderPath);
std::string URLPrefix = "file://";

std::string::size_type prefixIndex = shaderPathString.find(URLPrefix);

if (prefixIndex != std::string::npos) {
  spdlog::info("Found the file prefix");
  shaderPathString.erase(prefixIndex, URLPrefix.length());
} else {
  spdlog::error("File prefix not found");
}
#else
auto shaderPathString = std::string("../include/ScratchPadd/Graphics/GL/Shaders/") + shaderName + ".shader";
#endif

return shaderPathString;
}

class GL_View : public Graphics::View {
  private:
  std::unique_ptr<Graphics::FrameBuffer> frameBuffer_;
  std::unique_ptr<Graphics::VertexIndexBuffer> vertexIndexBuffer_;
  GL_Shader shader_;
  ImVec2 size_;
  std::string name_{"Unnamed"};
  public:
  GL_View() : frameBuffer_(std::make_unique<GL_FrameBuffer>()),    vertexIndexBuffer_(std::make_unique<GL_VertexIndexBuffer>()){}
  void setup(const std::string&& name) override {
    name_ = name;
    frameBuffer_->create(800, 600);
    vertexIndexBuffer_->create(vertices, indices);
    shader_.generate(getShaderPath("vertex"), getShaderPath("fragment"));
  }

  void setBackgroundColor(ImVec4 &backgroundColor) override {
    frameBuffer_->setBackgroundColor(backgroundColor);
  }

  ImVec4* getBackgroundColor() override {
    return frameBuffer_->getBackgroundColor();
  }

  void draw() override {
    shader_.use();
    // light_->update(shader_.get());

    frameBuffer_->bind();

    shader_.setVec4(shapeColor_,"uniform_color");
    shader_.update();
    vertexIndexBuffer_->draw();

    frameBuffer_->unbind();

    ImGui::Begin(name_.c_str());

    ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
    size_ = { viewportPanelSize.x, viewportPanelSize.y };

    // mCamera->set_aspect(mSize.x / mSize.y);
    // mCamera->update(mShader.get());

    // add rendered texture to ImGUI scene window
    uint64_t textureID = frameBuffer_->get();
    ImGui::Image(reinterpret_cast<void*>(textureID), ImVec2{ size_.x, size_.y }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

    ImGui::End();
  }

  void destroy() override {
    vertexIndexBuffer_->destroy();
  }

  virtual void reset() override {

  }
};




static std::unique_ptr<Graphics::View> ViewBuilder() {
  return std::make_unique<GL_View>();
}