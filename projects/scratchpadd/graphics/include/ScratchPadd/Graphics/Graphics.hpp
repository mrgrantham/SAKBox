#pragma once
#include "imgui.h"
#include <GL/glew.h>
#include <string>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <spdlog/spdlog.h>
#pragma clang diagnostic pop
#include <ScratchPadd/Graphics/Colors.hpp>
#include <ScratchPadd/Messages/Message.hpp>

class Graphics {
public:
  class View {
  protected:
    ImVec4 shapeColor_{ScratchPadd::Color::Green};

  public:
    virtual void setBackgroundColor(ImVec4 &backgroundColor) = 0;
    virtual ImVec4 *getBackgroundColor() = 0;
    ImVec4 *getShapeColor() { return &shapeColor_; }
    virtual void draw() = 0;
    virtual void destroy() = 0;
    virtual void reset() = 0;
    virtual void setup(const std::string &&name) = 0;
    virtual ~View() {}
  };

  class ControlView : public View {
  protected:
    ScratchPadd::MessageType::ControlSnapshot controls_;
    std::vector<std::string> orderedControlVector_;
    void buildControls(ScratchPadd::MessageType::ControlSnapshot &controls);

  public:
    void setBackgroundColor(ImVec4 &backgroundColor) override{};
    ImVec4 *getBackgroundColor() override { return {}; };
    void setControls(ScratchPadd::MessageType::ControlSnapshot &controls);
  };

  class ViewCollection : View {
  public:
    virtual void draw() {}
    virtual void destroy() {}
    virtual void reset() {}
    virtual void setup(const std::string &&name) {}
  };

  class FrameBuffer {
  protected:
    ImVec4 backgroundColor_{ScratchPadd::Color::Blue};

  public:
    ImVec4 *getBackgroundColor() { return &backgroundColor_; }
    void setBackgroundColor(ImVec4 &backgroundColor) {
      backgroundColor_ = backgroundColor;
    }
    virtual void create(int32_t width, int32_t height) = 0;
    virtual void destroy() = 0;
    virtual uint32_t get() = 0;
    virtual void bind() = 0;
    virtual void unbind() = 0;
    virtual ~FrameBuffer() {}
  };
  struct VertexPair {
    float position[3];
    float normal[3];
  };
  class VertexIndexBuffer {
  public:
    virtual void create(const std::vector<VertexPair> &vertices,
                        const std::vector<uint32_t> &indices) = 0;
    virtual void destroy() = 0;
    virtual void draw() = 0;
    virtual void bind() = 0;
    virtual void unbind() = 0;
    virtual ~VertexIndexBuffer() {}
  };

  virtual void setupWindow(const int width, const int height) = 0;
  virtual void addView(std::unique_ptr<Graphics::View> view) = 0;
  virtual bool draw() = 0;
  virtual void tearDown() = 0;
  virtual ~Graphics(){};
};

#if defined(SCRATCHPADD_GL)
#include <ScratchPadd/Graphics/GL/GL.hpp>
#elif defined(SCRATCHPADD_METAL)
#include <ScratchPadd/Graphics/Metal.hpp>
#elif defined(SCRATCHPADD_VULKAN)
#include <ScratchPadd/Graphics/Vulkan.hpp>
#else
#error "No Graphics Backend Selected"
#endif
