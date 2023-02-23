#pragma once

class GL_FrameBuffer : public Graphics::FrameBuffer {
  private:
    uint32_t frameBufferObject_ = 0;
    uint32_t textureID_ = 0;
    uint32_t depthID_ = 0;
    int32_t height_ = 0;
    int32_t width_ = 0;
  public:
    void create(int32_t width, int32_t height) override {
      width_ = width;
      height_ = height;

      if (frameBufferObject_) {
        destroy();
      }
      glGenFramebuffers(1, &frameBufferObject_);
      checkOpenGLErrors("glGenFramebuffers");
      glBindFramebuffer(GL_FRAMEBUFFER, frameBufferObject_);
      checkOpenGLErrors("glBindFramebuffer");

      glGenTextures(1, &textureID_);
      checkOpenGLErrors("glGenTextures");
      glBindTexture(GL_TEXTURE_2D, textureID_);
      checkOpenGLErrors("glBindTexture");

      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      checkOpenGLErrors("glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr)");
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID_, 0);

      glGenTextures(1, &depthID_);
      checkOpenGLErrors("glGenTextures(1, &depthID_)");
      glBindTexture(GL_TEXTURE_2D, depthID_);
      checkOpenGLErrors("glBindTexture(GL_TEXTURE_2D, depthID_)");

      glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, width_, height_);
      checkOpenGLErrors("glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, width_, height_)");

      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthID_, 0);
      checkOpenGLErrors("glFramebufferTexture2D");

      GLenum buffers[4] = { GL_COLOR_ATTACHMENT0 };
      glDrawBuffers(textureID_, buffers);
      checkOpenGLErrors("glDrawBuffers(textureID_, buffers)");

      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        spdlog::info("FrameBuffer is complete");
      } else {
        spdlog::error("FrameBuffer is not complete");
      }
      unbind();
    }

    void destroy() override {
      if (frameBufferObject_) {
        glDeleteFramebuffers(GL_FRAMEBUFFER, &frameBufferObject_);
        glDeleteTextures(1, &textureID_);
        glDeleteTextures(1, &depthID_);
        textureID_ = 0;
        depthID_ = 0;
      }
    }

    uint32_t get() override {
      return textureID_;
    }

    void bind() override {
      glBindFramebuffer(GL_FRAMEBUFFER, frameBufferObject_);
      checkOpenGLErrors("glBindFramebuffer(GL_FRAMEBUFFER, frameBufferObject_)");
      glViewport(0, 0, width_, height_);
      checkOpenGLErrors("glViewport");
      glClearColor(backgroundColor_.x * backgroundColor_.w, 
                    backgroundColor_.y * backgroundColor_.w, 
                    backgroundColor_.z * backgroundColor_.w, 
                    backgroundColor_.w);

      checkOpenGLErrors("glClearColor");
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      checkOpenGLErrors("glClear");
    }

    void unbind() override {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      checkOpenGLErrors("glBindFramebuffer(GL_FRAMEBUFFER, 0)");
    }
};