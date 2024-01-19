#include <Error.h>
#include <ShaderProgram.h>

#include <filesystem>
#include <fstream>

#include <spdlog/spdlog.h>

std::string GetFileContents(const std::string &pathString) {
  std::filesystem::path filePath(pathString);
  std::ifstream fileInputStream(filePath);
  return std::string((std::istreambuf_iterator<char>(fileInputStream)),
                     std::istreambuf_iterator<char>());
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

ShaderProgram::ShaderProgram(const std::string &vertexFile,
                             const std::string &fragmentFile) {
  std::string fragmentShaderSourceString = GetFileContents(fragmentFile);
  std::string vertexShaderSourceString = GetFileContents(vertexFile);

  const char *fragmentShaderSource = fragmentShaderSourceString.c_str();
  const char *vertexShaderSource = vertexShaderSourceString.c_str();

  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  CompileAndVerifyShader(vertexShader, vertexShaderSource);

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  CompileAndVerifyShader(fragmentShader, fragmentShaderSource);

  ID_ = glCreateProgram();
  glAttachShader(ID_, vertexShader);
  glAttachShader(ID_, fragmentShader);
  glLinkProgram(ID_);
  // Check that program linked successfully
  GLint linkOk;
  glGetProgramiv(ID_, GL_LINK_STATUS, &linkOk);
  if (!linkOk) {
    int statusLength = 0;
    glGetProgramiv(ID_, GL_INFO_LOG_LENGTH, &statusLength);
    auto message = std::make_unique<char[]>(statusLength);
    glGetProgramInfoLog(ID_, statusLength, &statusLength, message.get());
    spdlog::error("Error in glLinkProgram: {}", message.get());
  }
  glValidateProgram(ID_);
  checkOpenGLErrors("glLinkProgram");
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);
}

void ShaderProgram::activate() { glUseProgram(ID_); }
void ShaderProgram::destroy() { glDeleteProgram(ID_); }