

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

static std::string GetFilePath(const std::string &name, const std::string &extension,
                               const std::string &projectPath) {

  std::string workspace = "_main/"; // main repository runfiles directory under Bzlmod
  std::string project = "projects/examples/opengl_tutorial/";

  std::string relativePath = workspace + project + projectPath + name + "." + extension;
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

  std::string fragmentShaderFilePath = GetFilePath("shader", "frag", "resources/shaders/");
  std::string vertexShaderFilePath = GetFilePath("shader", "vert", "resources/shaders/");

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

  // Pyramid. Each face has its own vertices so it can carry its own normal.
  // The vertex tables are aligned in columns by hand.
  // clang-format off
  GLfloat vertices[] = {
      //  COORDINATES      //     COLORS        // TEX COORDS //   NORMALS   //
      -0.5f, 0.0f,  0.5f,  0.83f, 0.70f, 0.44f,  0.0f, 0.0f,   0.0f, -1.0f,  0.0f, // Bottom
      -0.5f, 0.0f, -0.5f,  0.83f, 0.70f, 0.44f,  0.0f, 5.0f,   0.0f, -1.0f,  0.0f,
       0.5f, 0.0f, -0.5f,  0.83f, 0.70f, 0.44f,  5.0f, 5.0f,   0.0f, -1.0f,  0.0f,
       0.5f, 0.0f,  0.5f,  0.83f, 0.70f, 0.44f,  5.0f, 0.0f,   0.0f, -1.0f,  0.0f,

      -0.5f, 0.0f,  0.5f,  0.83f, 0.70f, 0.44f,  0.0f, 0.0f,  -0.8f,  0.5f,  0.0f, // Left
      -0.5f, 0.0f, -0.5f,  0.83f, 0.70f, 0.44f,  5.0f, 0.0f,  -0.8f,  0.5f,  0.0f,
       0.0f, 0.8f,  0.0f,  0.92f, 0.86f, 0.76f,  2.5f, 5.0f,  -0.8f,  0.5f,  0.0f,

      -0.5f, 0.0f, -0.5f,  0.83f, 0.70f, 0.44f,  5.0f, 0.0f,   0.0f,  0.5f, -0.8f, // Back
       0.5f, 0.0f, -0.5f,  0.83f, 0.70f, 0.44f,  0.0f, 0.0f,   0.0f,  0.5f, -0.8f,
       0.0f, 0.8f,  0.0f,  0.92f, 0.86f, 0.76f,  2.5f, 5.0f,   0.0f,  0.5f, -0.8f,

       0.5f, 0.0f, -0.5f,  0.83f, 0.70f, 0.44f,  0.0f, 0.0f,   0.8f,  0.5f,  0.0f, // Right
       0.5f, 0.0f,  0.5f,  0.83f, 0.70f, 0.44f,  5.0f, 0.0f,   0.8f,  0.5f,  0.0f,
       0.0f, 0.8f,  0.0f,  0.92f, 0.86f, 0.76f,  2.5f, 5.0f,   0.8f,  0.5f,  0.0f,

       0.5f, 0.0f,  0.5f,  0.83f, 0.70f, 0.44f,  5.0f, 0.0f,   0.0f,  0.5f,  0.8f, // Front
      -0.5f, 0.0f,  0.5f,  0.83f, 0.70f, 0.44f,  0.0f, 0.0f,   0.0f,  0.5f,  0.8f,
       0.0f, 0.8f,  0.0f,  0.92f, 0.86f, 0.76f,  2.5f, 5.0f,   0.0f,  0.5f,  0.8f,
  };

  GLuint indices[] = {
      0,  1,  2,  // Bottom
      0,  2,  3,  // Bottom
      4,  6,  5,  // Left
      7,  9,  8,  // Back
      10, 12, 11, // Right
      13, 15, 14, // Front
  };

  GLfloat lightVertices[] = {
    -0.1f, -0.1f, 0.1f,
    -0.1f, -0.1f, -0.1f,
    0.1f, -0.1f, -0.1f,
    0.1f, -0.1f, 0.1f,
    -0.1f, 0.1f, 0.1f,
    -0.1f, 0.1f, -0.1f,
    0.1f, 0.1f, -0.1f,
    0.1f, 0.1f, 0.1f,
  };

  GLuint lightIndices[] = {
    0,1,2,
    0,2,3,
    0,4,7,
    0,7,3,
    3,7,6,
    3,6,2,
    2,6,5,
    2,5,1,
    1,5,4,
    1,4,0,
    4,5,6,
    4,6,7,
  };
  // clang-format on

  const unsigned int windowWidth = 800;
  const unsigned int windowHeight = 800;

  GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "Test Window", nullptr, nullptr);
  if (window == nullptr) {
    spdlog::info("Failed to create window");
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  GLenum status = glewInit();
  if (status != GLEW_OK) {
    std::string glewError = reinterpret_cast<const char *>(glewGetErrorString(status));
    spdlog::error("Errors: {}\n", glewError);
  }
  // can only get a printable version string for OpenGL
  // after a context was created and assigned
  char *version = (char *)glGetString(GL_VERSION);
  const GLubyte *renderer = glGetString(GL_RENDERER); // get renderer string
  spdlog::info("GL RENDERER: {}", (char *)renderer);
  spdlog::info("GL VERSION: {}", version);
  std::string glewVersion = reinterpret_cast<const char *>(glewGetString(GLEW_VERSION));
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
  vertexArrayObject.linkAttributes(vertexBufferObject, 0, 3, GL_FLOAT, 11 * sizeof(float),
                                   (void *)nullptr);
  // This one is for aColor since second arguement is 1 so (location = 1)
  vertexArrayObject.linkAttributes(vertexBufferObject, 1, 3, GL_FLOAT, 11 * sizeof(float),
                                   (void *)(3 * sizeof(float)));
  vertexArrayObject.linkAttributes(vertexBufferObject, 2, 2, GL_FLOAT, 11 * sizeof(float),
                                   (void *)(6 * sizeof(float)));
  vertexArrayObject.linkAttributes(vertexBufferObject, 3, 3, GL_FLOAT, 11 * sizeof(float),
                                   (void *)(8 * sizeof(float)));
  vertexArrayObject.unbind();
  vertexBufferObject.unbind();
  elementBufferObject.unbind();

  // Light handling

  ShaderProgram lightShader(GetFilePath("lighting", "vert", "resources/shaders/"),
                            GetFilePath("lighting", "frag", "resources/shaders/"));

  VertexArrayObject lightVAO;
  lightVAO.bind();
  VertexBufferObject lightVBO(lightVertices, sizeof(lightVertices));
  ElementBufferObject lightEBO(lightIndices, sizeof(lightIndices));

  lightVAO.linkAttributes(lightVBO, 0, 3, GL_FLOAT, 3 * sizeof(float), (void *)nullptr);

  lightVAO.unbind();
  lightVBO.unbind();
  lightEBO.unbind();

  glm::vec4 lightColor(1.0f, 0.5f, 0.0f, 1.0f);
  glm::vec3 lightPosition(0.5f, 0.5f, 0.5f);
  glm::mat4 lightModel(1.0f);
  lightModel = glm::translate(lightModel, lightPosition);

  glm::vec3 pyramidPosition(0.0f, 0.0f, 0.0f);
  glm::mat4 pyramidModel(1.0f);
  pyramidModel = glm::translate(pyramidModel, pyramidPosition);

  lightShader.activate();
  glUniformMatrix4fv(glGetUniformLocation(lightShader.ID(), "model"), 1, GL_FALSE,
                     glm::value_ptr(lightModel));
  glUniform4f(glGetUniformLocation(lightShader.ID(), "lightColor"), lightColor.x, lightColor.y,
              lightColor.z, lightColor.w);
  shaderProgram.activate();
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID(), "model"), 1, GL_FALSE,
                     glm::value_ptr(pyramidModel));
  glUniform4f(glGetUniformLocation(shaderProgram.ID(), "lightColor"), lightColor.x, lightColor.y,
              lightColor.z, lightColor.w);
  glUniform3f(glGetUniformLocation(shaderProgram.ID(), "lightPosition"), lightPosition.x,
              lightPosition.y, lightPosition.z);

  // Texture handling

  std::string popCatImagePath = GetFilePath("brick", "png", "resources/textures/");
  Texture brickTexture(popCatImagePath, GL_TEXTURE_2D, GL_TEXTURE0, GL_RGBA, GL_UNSIGNED_BYTE);

  brickTexture.textureUnit(shaderProgram, "tex0", 0);

  // Allows opengl to know which shaped to cover others when drawing
  glEnable(GL_DEPTH_TEST);

  Camera camera(windowWidth, windowHeight, glm::vec3(0.0f, 0.0f, 2.0f));

  while (!glfwWindowShouldClose(window)) {

    glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    camera.inputs(window);
    camera.updateMatrix(45.0f, 0.1f, 100.0f);

    shaderProgram.activate();
    camera.matrix(shaderProgram, "cameraMatrix");

    brickTexture.bind();
    vertexArrayObject.bind();
    glDrawElements(GL_TRIANGLES, sizeof(indices) / sizeof(int), GL_UNSIGNED_INT, nullptr);

    lightShader.activate();
    camera.matrix(lightShader, "cameraMatrix");
    lightVAO.bind();
    glDrawElements(GL_TRIANGLES, sizeof(lightIndices) / sizeof(int), GL_UNSIGNED_INT, nullptr);
    glfwSwapBuffers(window);

    // Take care of all GLFW events
    glfwPollEvents();
  }

  // Clean up
  vertexArrayObject.destroy();
  vertexBufferObject.destroy();
  elementBufferObject.destroy();
  brickTexture.destroy();
  shaderProgram.destroy();

  lightVAO.destroy();
  lightVBO.destroy();
  lightEBO.destroy();
  lightShader.destroy();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}