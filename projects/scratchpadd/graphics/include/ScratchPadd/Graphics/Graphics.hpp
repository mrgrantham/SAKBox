#pragma once
#include "imgui.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <spdlog/spdlog.h>
#pragma clang diagnostic pop
#include <ScratchPadd/Graphics/Colors.hpp>
#include <ScratchPadd/Messages/Message.hpp>
class GLFWwindow;
class Graphics {
public:
  class View {
  protected:
    ImVec4 shapeColor_{ScratchPadd::Color::Green};

  public:
    virtual void setWindow(GLFWwindow *window) = 0;
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
    virtual std::pair<int32_t, int32_t> getSize() = 0;
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
    float position[3] = {0.0f, 0.0f, 0.0f};
    float normal[3] = {0.0f, 0.0f, 0.0f};
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

  class ImguiView : public View {
  private:
    ImVec4 appBackground_ = ScratchPadd::Color::Light_Gray;
    bool show_demo_window = true;
    bool show_another_window = false;

  public:
    void setBackgroundColor(ImVec4 &backgroundColor) override {
      appBackground_ = backgroundColor;
    };
    ImVec4 *getBackgroundColor() override { return &appBackground_; };
    void draw() override {
      // 1. Show the big demo window (Most of the sample code is in
      // ImGui::ShowDemoWindow()! You can browse its code to learn more about
      // Dear ImGui!).
      if (show_demo_window)
        ImGui::ShowDemoWindow(&show_demo_window);

      // 2. Show a simple window that we create ourselves. We use a Begin/End
      // pair to created a named window.
      {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                       // and append into it.

        ImGui::Text("This is some useful text."); // Display some text (you can
                                                  // use a format strings too)
        ImGui::Checkbox("Demo Window",
                        &show_demo_window); // Edit bools storing our window
                                            // open/close state
        ImGui::Checkbox("Another Window", &show_another_window);

        ImGui::SliderFloat(
            "float", &f, 0.0f,
            1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3(
            "app background color",
            (float *)&appBackground_); // Edit 3 floats representing a color
        // ImGui::ColorEdit3(
        //     "gl window color",
        //     (float *)viewBackgroundColor); // Edit 3 floats representing a
        //     color
        // ImGui::ColorEdit3(
        //     "gl shape color",
        //     (float *)viewShapeColor); // Edit 3 floats representing a color

        if (ImGui::Button(
                "Button")) // Buttons return true when clicked (most widgets
                           // return true when edited/activated)
          counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        ImGui::End();
      }

      // 3. Show another simple window.
      if (show_another_window) {
        ImGui::Begin(
            "Another Window",
            &show_another_window); // Pass a pointer to our bool variable (the
                                   // window will have a closing button that
                                   // will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
          show_another_window = false;
        ImGui::End();
      }
    }
    void destroy() override {}

    void reset() override {}

    void setup(const std::string &&name) override {}
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
