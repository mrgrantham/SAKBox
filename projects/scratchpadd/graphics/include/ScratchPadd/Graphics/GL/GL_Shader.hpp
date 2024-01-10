#pragma once
#include <GL/glew.h>

#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

// const std::string vertexShader = "#version 330 core\n"
//                                  "\n"
//                                  "layout(location = 0) in vec4 position;\n"
//                                  "\n"
//                                  "void main()\n"
//                                  "{\n"
//                                  "   gl_Position = position;\n"
//                                  "}\n";

// const std::string fragmentShader = "#version 330 core\n"
//                                    "\n"
//                                    "layout(location = 0) out vec4 color;"
//                                    "\n"
//                                    "void main()\n"
//                                    "{\n"
//                                    "   color =  vec4(1.0,0.5,0.5,0.5);\n"
//                                    "}\n";

class GL_Shader {
private:
  unsigned int programID_{0};
  unsigned int compileShader(unsigned int type, const std::string &source) {
    unsigned int id = glCreateShader(type);
    const char *source_c_str = source.c_str();
    glShaderSource(id, 1, &source_c_str, nullptr);
    glCompileShader(id);

    int status;
    glGetShaderiv(id, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
      int status_length = 0;
      glGetShaderiv(id, GL_INFO_LOG_LENGTH, &status_length);
      auto message = std::make_unique<char[]>(status_length);
      glGetShaderInfoLog(id, status_length, &status_length, message.get());
      spdlog::error("FAILED TO COMPILE {} SHADER WITH ERROR:\n{}",
                    (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT",
                    message.get());
      glDeleteShader(id);
      return 0;
    }
    return id;
  }

public:
  void generate(const std::string &&vertexShaderFile,
                const std::string &&fragmentShaderFile) {
    spdlog::info("vertexShaderFile: {}", vertexShaderFile);
    spdlog::info("fragmentShaderFile: {}", fragmentShaderFile);

    std::filesystem::path vertexShaderFilePath(vertexShaderFile);
    spdlog::info("Vertex Shader Path: {}", vertexShaderFilePath.string());
    std::ifstream vertexShaderInputStream(vertexShaderFilePath);
    const std::string vertexShaderString(
        (std::istreambuf_iterator<char>(vertexShaderInputStream)),
        std::istreambuf_iterator<char>());
    spdlog::info("Vertex Shader: \n{}", vertexShaderString);

    std::filesystem::path fragmentShaderFilePath(fragmentShaderFile);
    std::error_code fragmentShaderFileError;
    spdlog::info("Fragment Shader Path: {}", fragmentShaderFilePath.string());
    std::ifstream fragmentShaderInputStream(fragmentShaderFilePath);
    const std::string fragmentShaderString(
        (std::istreambuf_iterator<char>(fragmentShaderInputStream)),
        std::istreambuf_iterator<char>());
    spdlog::info("Fragment Shader: \n{}", fragmentShaderString);

    programID_ = glCreateProgram();
    unsigned int v_shader = compileShader(GL_VERTEX_SHADER, vertexShaderString);
    unsigned int f_shader =
        compileShader(GL_FRAGMENT_SHADER, fragmentShaderString);
    glAttachShader(programID_, v_shader);
    glAttachShader(programID_, f_shader);
    glLinkProgram(programID_);
    GLint link_ok;
    glGetProgramiv(programID_, GL_LINK_STATUS, &link_ok);
    if (!link_ok) {
      int status_length = 0;
      glGetProgramiv(programID_, GL_INFO_LOG_LENGTH, &status_length);
      auto message = std::make_unique<char[]>(status_length);
      glGetProgramInfoLog(programID_, status_length, &status_length,
                          message.get());
      spdlog::error("Error in glLinkProgram: {}", message.get());
    }
    glValidateProgram(programID_);

    // now that the shaders are linked into a program
    // we can delete them
    glDeleteShader(v_shader);
    glDeleteShader(f_shader);
  }

  void update() {
    // setVec4(ScratchPadd::Color::Shape_Color,"uniform_color");
  }
  void setVec4(const ImVec4 &vec4, const std::string &name) {
    GLint myLoc = glGetUniformLocation(programID_, name.c_str());
    glProgramUniform4fv(programID_, myLoc, 1, (float *)&vec4);
  }

  void setFloat(const float &val, const std::string &name) {
    GLint myLoc = glGetUniformLocation(programID_, name.c_str());
    glProgramUniform1fv(programID_, myLoc, 1, (float *)&val);
  }

  void use() { glUseProgram(programID_); }

  void destroy() { glDeleteProgram(programID_); }
};
