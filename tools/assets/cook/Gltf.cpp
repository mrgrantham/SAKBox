#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "Gltf.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <memory>

namespace Cook {

namespace {

struct DataDeleter {
  void operator()(cgltf_data *data) const { cgltf_free(data); }
};

std::string upper(std::string text) {
  std::ranges::transform(text, text.begin(), [](unsigned char c) { return std::toupper(c); });
  return text;
}

// The node's custom property sak.platform (glTF extras), or ""
std::string platformOf(const cgltf_data *data, const cgltf_node &node) {
  cgltf_size size = 0;
  if (cgltf_copy_extras_json(data, &node.extras, nullptr, &size) != cgltf_result_success ||
      size == 0)
    return "";
  std::string json(size, '\0');
  cgltf_copy_extras_json(data, &node.extras, json.data(), &size);
  std::erase_if(json, [](unsigned char c) { return std::isspace(c); });
  const std::string key = "\"sak.platform\":\"";
  const auto at = json.find(key);
  if (at == std::string::npos)
    return "";
  const auto start = at + key.size();
  return json.substr(start, json.find('"', start) - start);
}

// Linear light to sRGB, as Blender stores colours and the scene layer draws
// them
float toSrgb(float linear) {
  linear = std::clamp(linear, 0.0f, 1.0f);
  return linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1 / 2.4f) - 0.055f;
}

bool append(const cgltf_primitive &primitive, Mesh &mesh, std::string &error) {
  if (primitive.type != cgltf_primitive_type_triangles) {
    error = "a primitive that isn't triangles";
    return false;
  }
  const cgltf_accessor *positions = nullptr, *normals = nullptr, *colours = nullptr;
  for (cgltf_size a = 0; a < primitive.attributes_count; ++a) {
    const cgltf_attribute &attribute = primitive.attributes[a];
    if (attribute.type == cgltf_attribute_type_position)
      positions = attribute.data;
    else if (attribute.type == cgltf_attribute_type_normal)
      normals = attribute.data;
    else if (attribute.type == cgltf_attribute_type_color && attribute.index == 0)
      colours = attribute.data;
  }
  if (!positions) {
    error = "a primitive without positions";
    return false;
  }
  const auto first = static_cast<std::uint32_t>(mesh.vertexCount());
  const cgltf_size count = positions->count;
  std::vector<float> values(count * 4);
  cgltf_accessor_unpack_floats(positions, values.data(), count * 3);
  mesh.positions.insert(mesh.positions.end(), values.begin(), values.begin() + count * 3);
  if (normals) {
    cgltf_accessor_unpack_floats(normals, values.data(), count * 3);
    mesh.normals.insert(mesh.normals.end(), values.begin(), values.begin() + count * 3);
  } else {
    mesh.normals.insert(mesh.normals.end(), count * 3, 0.0f); // worked out below
  }
  if (colours) {
    const cgltf_size components = cgltf_num_components(colours->type);
    cgltf_accessor_unpack_floats(colours, values.data(), count * components);
    for (cgltf_size v = 0; v < count; ++v) {
      for (cgltf_size c = 0; c < 3; ++c)
        mesh.colours.push_back(toSrgb(values[v * components + c]));
      mesh.colours.push_back(components == 4 ? values[v * 4 + 3] : 1.0f);
    }
  } else {
    mesh.colours.insert(mesh.colours.end(), count * 4, 1.0f);
  }
  if (primitive.indices) {
    for (cgltf_size i = 0; i < primitive.indices->count; ++i)
      mesh.indices.push_back(
          first + static_cast<std::uint32_t>(cgltf_accessor_read_index(primitive.indices, i)));
  } else {
    for (cgltf_size i = 0; i < count; ++i)
      mesh.indices.push_back(first + static_cast<std::uint32_t>(i));
  }
  if (!normals) {
    // Each vertex's normal from its triangles' (smooth, weighted by area)
    for (std::size_t t = 0; t < mesh.indices.size(); t += 3) {
      const std::uint32_t *corner = &mesh.indices[t];
      if (corner[0] < first)
        continue;
      const float *a = &mesh.positions[corner[0] * 3], *b = &mesh.positions[corner[1] * 3],
                  *c = &mesh.positions[corner[2] * 3];
      const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                  w[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
      const float n[3] = {u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2],
                          u[0] * w[1] - u[1] * w[0]};
      for (int k = 0; k < 3; ++k)
        for (int d = 0; d < 3; ++d)
          mesh.normals[corner[k] * 3 + d] += n[d];
    }
    for (std::size_t v = first; v < mesh.vertexCount(); ++v) {
      float *n = &mesh.normals[v * 3];
      const float length = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
      if (length > 0)
        for (int d = 0; d < 3; ++d)
          n[d] /= length;
    }
  }
  return true;
}

} // namespace

std::optional<Model> readModel(const std::string &path, const std::string &object,
                               const std::string &platform, std::string &error) {
  cgltf_options options{};
  cgltf_data *raw = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &raw) != cgltf_result_success) {
    error = "can't read " + path + " as glTF";
    return std::nullopt;
  }
  std::unique_ptr<cgltf_data, DataDeleter> data(raw);
  if (cgltf_load_buffers(&options, data.get(), path.c_str()) != cgltf_result_success) {
    error = "can't read the data in " + path;
    return std::nullopt;
  }

  // The platform's own version first, then the object itself
  const cgltf_node *chosen = nullptr;
  const std::string ownName = object + "_" + upper(platform);
  for (cgltf_size n = 0; n < data->nodes_count && !chosen; ++n) {
    const cgltf_node &node = data->nodes[n];
    const std::string name = node.name ? node.name : "";
    if (node.mesh && (name == ownName || (platformOf(data.get(), node) == platform &&
                                          name.starts_with(object) && name != object)))
      chosen = &node;
  }
  for (cgltf_size n = 0; n < data->nodes_count && !chosen; ++n) {
    const cgltf_node &node = data->nodes[n];
    if (node.mesh && node.name && object == node.name)
      chosen = &node;
  }
  if (!chosen) {
    error = "no object named " + object + " with a mesh in " + path;
    return std::nullopt;
  }

  Model model{.object = chosen->name};
  for (cgltf_size p = 0; p < chosen->mesh->primitives_count; ++p) {
    if (!append(chosen->mesh->primitives[p], model.mesh, error)) {
      error.insert(0, model.object + ": ");
      return std::nullopt;
    }
  }
  return model;
}

} // namespace Cook
