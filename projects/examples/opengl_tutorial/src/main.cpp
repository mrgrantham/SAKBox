// GLEW must be included before GLFW
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

int main(int argc, char** argv) {
    glfwInit();

    // This tells the OS which version of opengl you would like to use
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // This line is necessary for mac to work
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    int windowWidth = 800;
    int windowHeight = 800;

    GLFWwindow *window = glfwCreateWindow(windowWidth,windowHeight,"Test Window",NULL,NULL);
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

    glViewport(0,0,windowWidth,windowHeight);
    glClearColor(0.07f,0.13f,0.17f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);

    while (!glfwWindowShouldClose(window)) {
        // Take care of all GLFW events
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}