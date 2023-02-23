#pragma once

#include "imgui.h"

const float positions[] = {
     -0.5f, -0.5f,
      0.0f, 0.5f,
      0.5f, -0.5f
};

const std::vector<Graphics::VertexPair> vertices = {
      {{-0.5f, -0.5f, 0.0f},{0.0f, 0.0f, 0.0f}},
      {{0.0f, 0.5f, 0.0f},{0.0f, 0.0f, 0.0f}},
      {{0.5f, -0.5f, 0.0f},{0.0f, 0.0f, 0.0f}},
      {{1.0f, 0.5f, 0.0f},{0.0f, 0.0f, 0.0f}}
};

const std::vector<uint32_t> indices = {
  0, 1, 2, 1, 2, 3
};
