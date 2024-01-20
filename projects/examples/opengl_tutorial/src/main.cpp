

// GLEW must be included before GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <Camera.h>
#include <DataDepRetriever/DataDependencies.hpp>
#include <ElementBufferObject.h>
#include <Error.h>
#include <MacOSFix.h>
#include <ShaderProgram.h>
#include <Texture.h>
#include <VertexArrayObject.h>
#include <VertexBufferObject.h>

static std::string GetFilePath(const std::string &name,
                               const std::string &extension,
                               const std::string &projectPath) {

  std::string workspace = "monograntham/";
  std::string project = "projects/examples/opengl_tutorial/";

  std::string relativePath =
      workspace + project + projectPath + name + "." + extension;
  auto shaderPathString = DataDepRetriever::GetFullDependencyPath(relativePath);
  // #endif
  if (!shaderPathString) {
    spdlog::error("File not found at : {}", relativePath);
    exit(1);
  }
  return shaderPathString.value();
}

int main(int argc, char **argv) {

  // Makes data deps declared in BUILD file retrievable. Will hang
  // when GetFullDependencyPath is called if this is not called first
  DataDepRetriever::ConfigureDependencies(argv[0]);

  std::string fragmentShaderFilePath =
      GetFilePath("shader", "frag", "resources/shaders/");
  std::string vertexShaderFilePath =
      GetFilePath("shader", "vert", "resources/shaders/");

  glfwInit();

  // This tells the OS which version of opengl you would like to use
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // This line is necessary for mac to work
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  //     // Orange Triforce
  //   GLfloat vertices[] = {
  //       //          COORDINATES                             //   COLORS
  //       -0.5f,     -0.5f * float(sqrt(3)) / 3,    0.0f, 0.8f, 0.3f,
  //       0.02f, // Lower left corner
  //       0.5f,      -0.5f * float(sqrt(3)) / 3,    0.0f, 0.8f, 0.2f,
  //       0.02f, // Lower right corner
  //       0.0f,      0.5f * float(sqrt(3)) * 2 / 3, 0.0f, 1.0f, 0.6f,
  //       0.32f, // Upper corner
  //       -0.5f / 2, 0.5f * float(sqrt(3)) / 6,     0.0f, 0.9f, 0.45f,
  //       0.17f, // Inner left
  //       0.5f / 2,  0.5f * float(sqrt(3)) / 6,     0.0f, 0.9f, 0.45f,
  //       0.17f, // Inner Right
  //       0.0f,      -0.5f * float(sqrt(3)) / 3,    0.0f, 0.8f, 0.3f,
  //       0.02f // Inner down
  //   };

  //   GLuint indices[] = {
  //       0, 3, 5, // Lower left triangle
  //       3, 2, 4, // Lower right triangle
  //       5, 4, 1  // Upper triangle
  //   };

  // // Square
  // GLfloat vertices[] = {
  //     //          COORDINATES                             //   COLORS
  //     -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // Lower left corner
  //     -0.5f, 0.5f,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // Upper left corner
  //     0.5f,  0.5f,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // Upper right corner
  //     0.5f,  -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // Lower right corner
  // };

  // GLuint indices[] = {
  //     0, 2, 1, // Upper triangle
  //     0, 3, 2, // Lower triangle
  // };

  // Pyramid
  GLfloat vertices[] = {
      //  COORDINATES    //   COLORS    // TEX COORDS //
      -0.5f, 0.0f,  0.5f,  0.83f, 0.70f, 0.44f, 0.0f,  0.0f,  -0.5f, 0.0f,
      -0.5f, 0.83f, 0.70f, 0.44f, 5.0f,  0.0f,  0.5f,  0.0f,  -0.5f, 0.83f,
      0.70f, 0.44f, 0.0f,  0.0f,  0.5f,  0.0f,  0.5f,  0.83f, 0.70f, 0.44f,
      5.0f,  0.0f,  0.0f,  0.8f,  0.0f,  0.92f, 0.86f, 0.76f, 2.5f,  5.0f,
  };

  GLuint indices[] = {
      0, 1, 2, // Upper triangle
      0, 2, 3, // Lower triangle
      0, 1, 4, // Lower triangle
      1, 2, 4, // Lower triangle
      2, 3, 4, // Lower triangle
      3, 0, 4, // Lower triangle
  };

  const unsigned int windowWidth = 800;
  const unsigned int windowHeight = 800;

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
  fix_render_on_mac(window);

  ShaderProgram shaderProgram(vertexShaderFilePath, fragmentShaderFilePath);

  VertexArrayObject vertexArrayObject;
  vertexArrayObject.bind();
  VertexBufferObject vertexBufferObject(vertices, sizeof(vertices));
  ElementBufferObject elementBufferObject(indices, sizeof(indices));

  // second argument aligns with location value specified in vertex shader
  // inputs This one is for aPos since second arguement is 0 so (location = 0)
  vertexArrayObject.linkAttributes(vertexBufferObject, 0, 3, GL_FLOAT,
                                   8 * sizeof(float), (void *)0);
  // This one is for aColor since second arguement is 1 so (location = 1)
  vertexArrayObject.linkAttributes(vertexBufferObject, 1, 3, GL_FLOAT,
                                   8 * sizeof(float),
                                   (void *)(3 * sizeof(float)));
  vertexArrayObject.linkAttributes(vertexBufferObject, 2, 2, GL_FLOAT,
                                   8 * sizeof(float),
                                   (void *)(6 * sizeof(float)));
  vertexArrayObject.unbind();
  vertexBufferObject.unbind();
  elementBufferObject.unbind();

  // Texture handling

  std::string popCatImagePath =
      GetFilePath("brick", "png", "resources/textures/");
  Texture popCatTexture(popCatImagePath, GL_TEXTURE_2D, GL_TEXTURE0, GL_RGBA,
                        GL_UNSIGNED_BYTE);

  popCatTexture.textureUnit(shaderProgram, "tex0", 0);

  // Allows opengl to know which shaped to cover others when drawing
  glEnable(GL_DEPTH_TEST);

  Camera camera(windowWidth, windowHeight, glm::vec3(0.0f, 0.0f, 2.0f));

  while (!glfwWindowShouldClose(window)) {

    glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shaderProgram.activate();

    camera.inputs(window);
    camera.matrix(45.0f, 0.1f, 100.0f, shaderProgram, "cameraMatrix");
    popCatTexture.bind();
    vertexArrayObject.bind();
    glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(int), GL_UNSIGNED_INT,
                   0);
    glfwSwapBuffers(window);

    // Take care of all GLFW events
    glfwPollEvents();
  }

  // Clean up
  vertexArrayObject.destroy();
  vertexBufferObject.destroy();
  popCatTexture.destroy();
  shaderProgram.destroy();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}