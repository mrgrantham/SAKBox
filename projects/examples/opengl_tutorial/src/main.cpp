

// GLEW must be included before GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <DataDepRetriever/DataDependencies.hpp>
#include <ElementBufferObject.h>
#include <Error.h>
#include <Shader.h>
#include <VertexArrayObject.h>
#include <VertexBufferObject.h>

static std::string GetShaderPath(const std::string &name,
                                 const std::string &extension,
                                 const std::string &dirPath) {

  std::string workspace = "monograntham";

  std::string relativePath = workspace + dirPath + name + "." + extension;
  auto shaderPathString = DataDepRetriever::GetFullDependencyPath(relativePath);
  // #endif
  if (!shaderPathString) {
    spdlog::error("Shader file not found at : {}", relativePath);
    exit(1);
  }
  return shaderPathString.value();
}

int main(int argc, char **argv) {

  // Makes data deps declared in BUILD file retrievable. Will hang
  // when GetFullDependencyPath is called if this is not called first
  DataDepRetriever::ConfigureDependencies(argv[0]);

  std::string fragmentShaderFilePath = GetShaderPath(
      "shader", "frag", "/projects/examples/opengl_tutorial/shaders/");
  std::string vertexShaderFilePath = GetShaderPath(
      "shader", "vert", "/projects/examples/opengl_tutorial/shaders/");

  glfwInit();

  // This tells the OS which version of opengl you would like to use
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // This line is necessary for mac to work
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLfloat vertices[] = {
      -0.5f,     -0.5f * float(sqrt(3)) / 3,    0.0f, // Lower left corner
      0.5f,      -0.5f * float(sqrt(3)) / 3,    0.0f, // Lower right corner
      0.0f,      0.5f * float(sqrt(3)) * 2 / 3, 0.0f, // Upper corner
      -0.5f / 2, 0.5f * float(sqrt(3)) / 6,     0.0f, // Inner left
      0.5f / 2,  0.5f * float(sqrt(3)) / 6,     0.0f, // Inner Right
      0.0f,      -0.5f * float(sqrt(3)) / 3,    0.0f, // Inner down
  };

  GLuint indices[] = {
      0, 3, 5, // Lower left triangle
      3, 2, 4, // Lower right triangle
      5, 4, 1  // Upper triangle
  };

  int windowWidth = 800;
  int windowHeight = 800;

  GLFWwindow *window =
      glfwCreateWindow(windowWidth, windowHeight, "Test Window", NULL, NULL);
  if (window == NULL) {
    spdlog::info("Failed to create window");
    glfwTerminate();
    return -1;
  }
  glfwMakeContextCurrent(window);

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

  glViewport(0, 0, windowWidth, windowHeight);

  Shader shader(vertexShaderFilePath, fragmentShaderFilePath);

  VertexArrayObject vertexArrayObject;
  vertexArrayObject.bind();
  VertexBufferObject vertexBufferObject(vertices, sizeof(vertices));
  ElementBufferObject elementBufferObject(indices, sizeof(indices));

  vertexArrayObject.linkVertexBufferObject(vertexBufferObject, 0);
  vertexArrayObject.unbind();

  vertexBufferObject.unbind();
  elementBufferObject.unbind();

  checkOpenGLErrors("elementBufferObject");

  glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glfwSwapBuffers(window);

  while (!glfwWindowShouldClose(window)) {

    glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    shader.activate();
    vertexArrayObject.bind();
    glDrawElements(GL_TRIANGLES, 9, GL_UNSIGNED_INT, 0);
    glfwSwapBuffers(window);

    // Take care of all GLFW events
    glfwPollEvents();
  }

  // Clean up
  vertexArrayObject.destroy();
  vertexBufferObject.destroy();
  shader.destroy();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}