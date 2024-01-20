#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <ShaderProgram.h>

class Camera {
private:
  glm::vec3 position_;
  glm::vec3 orientation_{0.0f, 0.0f, -1.0f};
  glm::vec3 up_{0.0f, 1.0f, 0.0f};

  bool firstClick_{true};

  int width_;
  int height_;

  float speed_ = 0.1f;
  float sensitivity_ = 100.0f;

public:
  Camera(int width, int height, glm::vec3 position);

  void matrix(float FOVdegrees, float nearPlane, float farPlane,
              ShaderProgram &shaderProgram, const char *uniform);
  void inputs(GLFWwindow *window);
};