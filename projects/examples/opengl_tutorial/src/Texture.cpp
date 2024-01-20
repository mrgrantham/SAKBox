#include <Texture.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

Texture::Texture(const std::string &image, GLenum textureType, GLenum slot,
                 GLenum format, GLenum pixelType) {
  type_ = textureType;
  // These will be populated with the metadata of the image we load
  int imageWidth, imageHeight, colorChannelCount;
  // OpenGL reads image data in different order from stb
  // This setting flips the data to display the image correctly
  stbi_set_flip_vertically_on_load(true);
  unsigned char *imageBytes = stbi_load(image.c_str(), &imageWidth,
                                        &imageHeight, &colorChannelCount, 0);

  glGenTextures(1, &ID_);
  glActiveTexture(slot);
  glBindTexture(type_, ID_);

  // This determines how the texture will look when we resize it. The rules for
  // generating new pixels
  glTexParameteri(type_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(type_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // This governs how the texture will display if the area is larger than the
  // texture size. The axis names for texture coordinates use S,T,R which is
  // similar to X,Y,Z. GL_REPEAT sets the texture to repeat along the S axis
  glTexParameteri(type_, GL_TEXTURE_WRAP_S, GL_REPEAT);
  // GL_REPEAT sets the texture to repeat along the T axis
  glTexParameteri(type_, GL_TEXTURE_WRAP_T, GL_REPEAT);

  glTexImage2D(type_, 0, format, imageWidth, imageHeight, 0, format, pixelType,
               imageBytes);
  glGenerateMipmap(type_);

  stbi_image_free(imageBytes);
  glBindTexture(type_, 0);
}

void Texture::textureUnit(ShaderProgram &shaderProgram, const char *uniform,
                          GLuint unit) {
  GLuint tex0Uniform = glGetUniformLocation(shaderProgram.ID(), "tex0");
  shaderProgram.activate();
  glUniform1i(tex0Uniform, 0);
}
void Texture::bind() { glBindTexture(type_, ID_); }
void Texture::unbind() { glBindTexture(type_, 0); }
void Texture::destroy() { glDeleteTextures(1, &ID_); }

GLuint Texture::ID() { return ID_; }