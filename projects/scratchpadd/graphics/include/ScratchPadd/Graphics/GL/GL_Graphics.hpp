#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "GL_Shader.hpp"
#include "GL_View.hpp"
#include "imgui.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

class GL_Graphics : public Graphics {
  ImVec4 appBackground = ScratchPadd::Color::Black;
  ImVec4 *viewShapeColor = nullptr;
  ImVec4 *viewBackgroundColor = nullptr;

  // Our state
  const char *glsl_version = "#version 330 core";
  GL_Shader shader_;
  std::vector<std::unique_ptr<View>> views_;

public:
  GLFWwindow *window;
  void setupWindow(const int width, const int height) override {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
      spdlog::error("Could not init GLFW!");
      exit(-1);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(width, height, PROJECT_NAME, nullptr, nullptr);
    if (!window) {
      spdlog::error("No window created!");
      glfwTerminate();
      exit(-1);
    }
    spdlog::info("Window Created!");
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    glewExperimental = GL_TRUE;
    GLenum status = glewInit();
    if (status != GLEW_OK) {
      std::string glewError =
          reinterpret_cast<const char *>(glewGetErrorString(status));
      spdlog::error("Errors: {}\n", glewError);
    }
    // can only get a printable version string for OpenGL
    // after a context was created and assigned
    char *version = (char *)glGetString(GL_VERSION);
    const GLubyte *renderer = glGetString(GL_RENDERER); // get renderer string
    spdlog::info("GL RENDERER: {}", (char *)renderer);
    spdlog::info("GL VERSION: {}", version);
    std::string glewVersion =
        reinterpret_cast<const char *>(glewGetString(GLEW_VERSION));
    spdlog::info("Status: Using GLEW {}\n", glewVersion);

    // This was based on the back view that no longer is set here
    // making the view contained empty and the following lines seg fault
    // viewBackgroundColor = views_.back()->getBackgroundColor();
    // viewShapeColor = views_.back()->getShapeColor();
  }

  void addView(std::unique_ptr<Graphics::View> view) override {
    view->setWindow(window);
    views_.push_back(std::move(view));
  }

  bool draw() override {
    if (!glfwWindowShouldClose(window)) {
      // checkOpenGLErrors();
      glfwPollEvents();

      // Start the Dear ImGui frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      for (auto &&view : views_) {
        view->draw();
      }
      checkOpenGLErrors("After draw");

      // Rendering
      ImGui::Render();
      int display_w, display_h;
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(appBackground.x * appBackground.w,
                   appBackground.y * appBackground.w,
                   appBackground.z * appBackground.w, appBackground.w);
      glClear(GL_COLOR_BUFFER_BIT);
      checkOpenGLErrors("before glDrawArrays");
      // glDrawArrays(GL_TRIANGLES, 0, 3);
      checkOpenGLErrors("after glDrawArrays");

      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      glfwSwapBuffers(window);
      return true;
    }
    return false;
  }

  void tearDown() override {
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    for (auto &&view : views_) {
      view->destroy();
    }
    shader_.destroy();
    glfwDestroyWindow(window);
    glfwTerminate();
  }
};

static std::unique_ptr<Graphics> GraphicsBuilder() {
  return std::make_unique<GL_Graphics>();
}