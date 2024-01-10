#pragma once

#include "imgui.h"

namespace SampleItems {
const float positions[] = {-0.5f, -0.5f, 0.0f, 0.5f, 0.5f, -0.5f};

const std::vector<Graphics::VertexPair> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {{-0.25f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {{0.25f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}}};

const std::vector<uint32_t> indices = {0, 1, 2, 1, 2, 3};

class Circle {
private:
  Graphics::VertexPair center_;
  float radius_;
  int sides_;
  std::vector<uint32_t> indices_;
  std::vector<Graphics::VertexPair> vertices_;

  std::vector<uint32_t> computeIndices() const {
    std::vector<uint32_t> indices;
    bool loopAround = false;
    for (int i = 1; i < vertices_.size(); i++) {
      indices.push_back(i);
      if (i + 1 >= vertices_.size()) {
        indices.push_back(1); // wrap around to first outter vertex
      } else {
        indices.push_back(i + 1);
      }
      indices.push_back(0);
    }
    fmt::print("indices: [");
    for (auto i : indices) {
      fmt::print(" {},", i);
    }
    fmt::print("]\n");
    return indices;
  }

  std::vector<Graphics::VertexPair> computeVertices() const {

    std::vector<Graphics::VertexPair> vertices;
    // Calculate the angle between each side
    double angle = 2 * M_PI / sides_;

    vertices.push_back(center_);

    // Calculate and print the coordinates of each vertex
    std::cout << "Vertices of the " << sides_ << "-sided shape:" << std::endl;
    for (size_t i = 0; i < sides_; ++i) {
      double x = radius_ * cos(angle * i);
      double y = radius_ * sin(angle * i);
      Graphics::VertexPair vertex;
      vertex.position[0] = x;
      vertex.position[1] = y;
      vertices.push_back(vertex);
    }
    return vertices;
  }

public:
  Circle(float centerX, float centerY, float radius, int sides)
      : center_(), radius_(radius), sides_(sides) {
    // indices are determined based on this compute so vetices must be computed
    // first
    center_.position[0] = centerX;
    center_.position[1] = centerY;
    vertices_ = computeVertices();
    indices_ = computeIndices();
  }

  std::vector<Graphics::VertexPair> vertices() const { return vertices_; }
  std::vector<uint32_t> indices() const { return indices_; }
};

} // namespace SampleItems