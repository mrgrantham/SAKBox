#include <filesystem>
#include <fstream>

// GLEW must be included before GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

#include <DataDepRetriever/DataDependencies.hpp>

static void checkOpenGLErrors(const std::string &&name = "") {
  while (GLenum error = glGetError()) {
    spdlog::error("OPENGL [{0}] ERROR: (0x{1:x})", name, error);
  }
}

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

static void CompileAndVerifyShader(GLuint shader, const char *shaderSource) {
  glShaderSource(shader, 1, &shaderSource, NULL);
  glCompileShader(shader);
  int status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    int status_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &status_length);
    auto message = std::make_unique<char[]>(status_length);
    glGetShaderInfoLog(shader, status_length, &status_length, message.get());
    spdlog::error("FAILED TO COMPILE SHADER WITH ERROR:\n{}", message.get());
  } else {
    spdlog::info("Successful compile of shader");
  }
}

static std::string GetFileContents(const std::string &pathString) {
  std::filesystem::path filePath(pathString);
  std::ifstream fileInputStream(filePath);
  return std::string((std::istreambuf_iterator<char>(fileInputStream)),
                     std::istreambuf_iterator<char>());
}

int main(int argc, char **argv) {

  // Makes data deps declared in BUILD file retrievable. Will hang
  // when GetFullDependencyPath is called if this is not called first
  DataDepRetriever::ConfigureDependencies(argv[0]);

  std::string fragmentShaderSourceString = GetFileContents(GetShaderPath(
      "shader", "frag", "/projects/examples/opengl_tutorial/shaders/"));
  std::string vertexShaderSourceString = GetFileContents(GetShaderPath(
      "shader", "vert", "/projects/examples/opengl_tutorial/shaders/"));

  const char *fragmentShaderSource = fragmentShaderSourceString.c_str();
  const char *vertexShaderSource = vertexShaderSourceString.c_str();

  glfwInit();

  // This tells the OS which version of opengl you would like to use
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // This line is necessary for mac to work
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLfloat vertices[] = {
      -0.5f, -0.5f * float(sqrt(3)) / 3,    0.0f,
      0.5f,  -0.5f * float(sqrt(3)) / 3,    0.0f,
      0.0f,  0.5f * float(sqrt(3)) * 2 / 3, 0.0f,
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

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  CompileAndVerifyShader(vertexShader, vertexShaderSource);

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  CompileAndVerifyShader(fragmentShader, fragmentShaderSource);

  GLuint shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);
  // Check that program linked successfully
  GLint linkOk;
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkOk);
  if (!linkOk) {
    int statusLength = 0;
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &statusLength);
    auto message = std::make_unique<char[]>(statusLength);
    glGetProgramInfoLog(shaderProgram, statusLength, &statusLength,
                        message.get());
    spdlog::error("Error in glLinkProgram: {}", message.get());
  }
  glValidateProgram(shaderProgram);
  checkOpenGLErrors("glLinkProgram");

  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  GLuint vertexArrayObject, vertexBufferObject;

  glGenVertexArrays(1, &vertexArrayObject);
  glGenBuffers(1, &vertexBufferObject);

  glBindVertexArray(vertexArrayObject);
  checkOpenGLErrors("glBindVertexArray");

  glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject);
  checkOpenGLErrors("glBindBuffer");
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  checkOpenGLErrors("glBufferData");

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  checkOpenGLErrors("glEnableVertexAttribArray");

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  checkOpenGLErrors("glBindVertexArray");

  glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glfwSwapBuffers(window);

  while (!glfwWindowShouldClose(window)) {

    glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shaderProgram);
    glBindVertexArray(vertexArrayObject);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glfwSwapBuffers(window);

    // Take care of all GLFW events
    glfwPollEvents();
  }

  // Clean up
  glDeleteVertexArrays(1, &vertexArrayObject);
  glDeleteBuffers(1, &vertexBufferObject);
  glDeleteProgram(shaderProgram);

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}