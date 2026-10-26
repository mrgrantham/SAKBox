#pragma once
#include "imgui.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <ScratchPadd/Graphics/Colors.hpp>
#include <ScratchPadd/Graphics/Theme.hpp>
#include <ScratchPadd/Messages/Message.hpp>
#include <functional>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>
struct GLFWwindow; // declared as a struct by GLFW
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

  // A panel showing one padd's controls as widgets. Edits update the panel's
  // copy immediately and are reported through the change handler; the owning
  // padd's reply (a new snapshot passed to setControls) confirms or corrects
  // them.
  class ControlView : public View {
  public:
    using ChangeHandler = std::function<void(const ScratchPadd::MessageType::ControlChange &)>;

  protected:
    ScratchPadd::MessageType::ControlSnapshot controls_;
    std::vector<std::string> orderedControlVector_; // control names, sorted
    ChangeHandler onChange_;

    // Draws a widget for each control: sliders for ranged numbers, input
    // boxes otherwise, a checkbox for booleans, and a combo box or text field
    // for strings.
    void buildControls();

  public:
    void setBackgroundColor(ImVec4 &backgroundColor) override {};
    ImVec4 *getBackgroundColor() override { return {}; };
    // Shows `controls`, replacing whatever was shown before.
    void setControls(const ScratchPadd::MessageType::ControlSnapshot &controls);
    // Called with a ControlChange each time the user edits a control (the
    // padd that owns the panel sends it to the padd whose controls it shows).
    void setOnChange(ChangeHandler onChange) { onChange_ = std::move(onChange); }
    // Where the panel first appears, as a fraction of the window: (0, 0) is
    // the top-left corner, (1, 0) the top-right. Without this, ImGui picks
    // a spot. The user can still move the panel.
    void setInitialPlacement(ImVec2 placement) { placement_ = placement; }

  protected:
    std::optional<ImVec2> placement_;
  };

  class ViewCollection : View {
  public:
    void draw() override {}
    void destroy() override {}
    void reset() override {}
    void setup(const std::string &&name) override {}
  };

  class FrameBuffer {
  protected:
    ImVec4 backgroundColor_{ScratchPadd::Color::Blue};

  public:
    virtual std::pair<int32_t, int32_t> getSize() = 0;
    ImVec4 *getBackgroundColor() { return &backgroundColor_; }
    void setBackgroundColor(ImVec4 &backgroundColor) { backgroundColor_ = backgroundColor; }
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
    void setWindow(GLFWwindow *window) override {}
    void setBackgroundColor(ImVec4 &backgroundColor) override { appBackground_ = backgroundColor; };
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

        ImGui::SliderFloat("float", &f, 0.0f,
                           1.0f); // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("app background color",
                          (float *)&appBackground_); // Edit 3 floats representing a color
        // ImGui::ColorEdit3(
        //     "gl window color",
        //     (float *)viewBackgroundColor); // Edit 3 floats representing a
        //     color
        // ImGui::ColorEdit3(
        //     "gl shape color",
        //     (float *)viewShapeColor); // Edit 3 floats representing a color

        if (ImGui::Button("Button")) // Buttons return true when clicked (most widgets
                                     // return true when edited/activated)
          counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
      }

      // 3. Show another simple window.
      if (show_another_window) {
        ImGui::Begin("Another Window",
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
  // Destroys and removes `view`, which addView() was given. Call it between
  // frames, on the thread that draws.
  virtual void removeView(const Graphics::View *view) = 0;
  virtual bool draw() = 0;
  virtual void tearDown() = 0;
  virtual ~Graphics() {};
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
