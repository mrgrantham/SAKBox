#pragma once

class GL_VertexIndexBuffer: public Graphics::VertexIndexBuffer {
  private:
    GLuint vertexBufferObject_{0};
    GLuint vertexArrayObject_{0};
    GLuint indexBufferObject_{0};
    size_t indexCount{0};
  public:
    void create(const std::vector<Graphics::VertexPair>& vertices, const std::vector<uint32_t>& indices) override {
      indexCount = indices.size();
      spdlog::info("indexCount: {}",indexCount);
      glGenVertexArrays(1, &vertexArrayObject_);

      glGenBuffers(1, &indexBufferObject_);
      glGenBuffers(1, &vertexBufferObject_);

      glBindVertexArray(vertexArrayObject_);

      glBindBuffer(GL_ARRAY_BUFFER, vertexBufferObject_);
      glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Graphics::VertexPair), vertices.data(), GL_STATIC_DRAW);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferObject_);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

      glEnableVertexAttribArray(0); // enable location = 0 in vertex shader (position)
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Graphics::VertexPair), (void*)0);

      glEnableVertexAttribArray(1); // enable location = 1 in vertex shader (aNormal)
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Graphics::VertexPair), (void*)offsetof(Graphics::VertexPair, normal));

      glBindVertexArray(0);
    }

    void destroy() override {
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
      glDeleteBuffers(1, &vertexBufferObject_);
      glDeleteBuffers(1, &indexBufferObject_);
      glDeleteVertexArrays(1, &vertexArrayObject_);
    }

    void draw() override {
      bind();
      checkOpenGLErrors(std::string("Before: ") + __PRETTY_FUNCTION__);
      // the vertices as line loop
      glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
      // glDrawArrays(GL_TRIANGLES,0, indexCount);
      checkOpenGLErrors(std::string("After: ") + __PRETTY_FUNCTION__);

      unbind();
    }

    void bind() override {
      glBindVertexArray(vertexArrayObject_);
    }

    void unbind() override {
      glBindVertexArray(0);
    }
};