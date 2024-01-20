#include <Camera.h>

Camera::Camera(int width, int height, glm::vec3 position)
    : position_(position), width_(width), height_(height) {}

void Camera::matrix(float FOVdegrees, float nearPlane, float farPlane,
                    ShaderProgram &shaderProgram, const char *uniform) {
  glm::mat4 view(1.0f);
  glm::mat4 projection(1.0f);

  view = glm::lookAt(position_, position_ + orientation_, up_);
  projection =
      glm::perspective(glm::radians(FOVdegrees),
                       (float)((float)width_ / height_), nearPlane, farPlane);

  glUniformMatrix4fv(glGetUniformLocation(shaderProgram.ID(), uniform), 1,
                     GL_FALSE, glm::value_ptr(projection * view));
}
void Camera::inputs(GLFWwindow *window) {
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
    position_ += speed_ * orientation_;
  }
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
    position_ += speed_ * -glm::normalize(glm::cross(orientation_, up_));
  }
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
    position_ += speed_ * -orientation_;
  }
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
    position_ += speed_ * glm::normalize(glm::cross(orientation_, up_));
  }
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    position_ += speed_ * up_;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
    position_ += speed_ * -up_;
  }
  if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
    speed_ = 0.4f;
  } else if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_RELEASE) {
    speed_ = 0.1f;
  }

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

    if (firstClick_) {
      glfwSetCursorPos(window, (width_ / 2), (height_ / 2));
      firstClick_ = false;
    }

    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    // Normalize coords to center of screen
    float rotationX = sensitivity_ * (float)(mouseY - (height_ / 2)) / height_;
    float rotationY = sensitivity_ * (float)(mouseX - (width_ / 2)) / width_;

    glm::vec3 newOrientation =
        glm::rotate(orientation_, glm::radians(-rotationX),
                    glm::normalize(glm::cross(orientation_, up_)));

    if (!((glm::angle(newOrientation, up_) <= glm::radians(5.0f)) ||
          (glm::angle(newOrientation, -up_) <= glm::radians(5.0f)))) {
      orientation_ = newOrientation;
    }

    orientation_ = glm::rotate(orientation_, glm::radians(-rotationY), up_);
    glfwSetCursorPos(window, (width_ / 2), (height_ / 2));
  } else if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) ==
             GLFW_RELEASE) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    firstClick_ = true;
  }
}