// Scene::Renderer on Vulkan, drawing what the other desktop backends draw,
// the same way: one shader for every material (lit or unlit, vertex colours
// times the material's colour, fog), translucent meshes kept to the end of
// the frame and drawn farthest first, and the overlay drawn last, in screen
// points. Each frame is drawn 4x multisampled into the window's own images
// (Surface.hpp), then shown. The shaders are the Metal backend's, in
// Vulkan's GLSL, compiled when the renderer's made.
#include "Backend.hpp"

#include <ScratchPadd/Scene/OverlayText.hpp>
#include <ScratchPadd/Scene/Spirv/GlslToSpirv.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

namespace ScratchPadd::Scene {

namespace {

// Frame matches Backend::FrameUniforms; Draw, the push constants,
// DrawConstants below. y is up in clip space, as OpenGL has it: the
// viewport is flipped to match.
const char *kSceneVertex = R"(#version 450
layout(set = 0, binding = 0) uniform Frame {
  mat4 viewProj;
  vec2 viewport;
  vec2 pad0;
  vec4 ambient;
  vec4 lightDirections[4];
  vec4 lightColours[4];
  vec4 fogColour;
  float fogDensity;
  int lightCount;
  vec2 pad1;
} frame;
layout(push_constant) uniform Draw {
  mat4 model;
  vec4 colour;      // the material's: colour, and alpha 0-1
  float worldSnap;  // world grid vertices snap to (0: off)
  float screenSnap; // pixel grid vertices snap to (0: off)
  int lit;
  int toonBands;    // lit in this many bands (toon shading), or 0: smoothly
  float extrude;    // an outline's hull: pushed out this far along the normals (0: not one)
  int outlinePass;  // an outline's hull: the colour alone
  float outlined;   // screen-space outlines: 1 for a mesh that has one
  int textured;     // the material's texture multiplies its colour
} draw;
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 colour;
layout(location = 3) in vec2 uv;
layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec4 vColour;
layout(location = 2) out float vDistance;
layout(location = 3) out vec2 vTexCoord;
void main() {
  vec4 world = draw.model * vec4(position, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(draw.model)));
  if (draw.extrude > 0.0)
    world.xyz += normalize(normalMatrix * normal) * draw.extrude;
  if (draw.worldSnap > 0.0)
    world.xyz = floor(world.xyz / draw.worldSnap + 0.5) * draw.worldSnap;
  vec4 clip = frame.viewProj * world;
  if (draw.screenSnap > 0.0 && clip.w > 0.0) {
    vec2 pixel = (clip.xy / clip.w * 0.5 + 0.5) * frame.viewport;
    pixel = floor(pixel / draw.screenSnap + 0.5) * draw.screenSnap;
    clip.xy = (pixel / frame.viewport * 2.0 - 1.0) * clip.w;
  }
  gl_Position = clip;
  vNormal = normalMatrix * normal;
  vColour = colour;
  vDistance = clip.w;
  vTexCoord = uv;
}
)";

const char *kSceneFragment = R"(#version 450
layout(set = 0, binding = 0) uniform Frame {
  mat4 viewProj;
  vec2 viewport;
  vec2 pad0;
  vec4 ambient;
  vec4 lightDirections[4];
  vec4 lightColours[4];
  vec4 fogColour;
  float fogDensity;
  int lightCount;
  vec2 pad1;
} frame;
layout(push_constant) uniform Draw {
  mat4 model;
  vec4 colour;
  float worldSnap;
  float screenSnap;
  int lit;
  int toonBands;
  float extrude;
  int outlinePass;
  float outlined;
  int textured;
} draw;
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec4 vColour;
layout(location = 2) in float vDistance;
layout(location = 3) in vec2 vTexCoord;
// The material's texture (set 1; a 1 x 1 white one when it has none)
layout(set = 1, binding = 0) uniform sampler2D image;
layout(location = 0) out vec4 outColour;
#ifdef WITH_SURFACE
// Screen-space outlines' second target: the surface's direction, and its
// distance from the eye, negative where the mesh isn't outlined
layout(location = 1) out vec4 outSurface;
#endif
void main() {
#ifdef WITH_SURFACE
  outSurface = vec4(normalize(vNormal), draw.outlined > 0.5 ? vDistance : -vDistance);
#endif
  vec4 texel = draw.textured != 0 && draw.outlinePass == 0 ? texture(image, vTexCoord) : vec4(1.0);
  vec3 colour = draw.outlinePass != 0 ? draw.colour.rgb : vColour.rgb * draw.colour.rgb * texel.rgb;
  if (draw.lit != 0) {
    vec3 n = normalize(vNormal);
    vec3 light = frame.ambient.rgb;
    for (int i = 0; i < frame.lightCount; ++i) {
      float lit = max(dot(n, -frame.lightDirections[i].xyz), 0.0);
      if (draw.toonBands > 1) // (as Types.hpp's toonStep)
        lit = clamp(floor(lit * float(draw.toonBands)) / float(draw.toonBands - 1), 0.0, 1.0);
      light += frame.lightColours[i].rgb * lit;
    }
    colour *= light;
  }
  float fog = clamp(1.0 - exp(-frame.fogDensity * vDistance), 0.0, 1.0);
  outColour = vec4(mix(colour, frame.fogColour.rgb, fog), vColour.a * draw.colour.a * texel.a);
}
)";

// The edge pass for screen-space outlines (as the OpenGL and Metal
// backends'): one triangle over the screen; the frame with the outline's
// colour wherever a pixel a few away is much nearer or farther, or faces a
// different way, and either is on an outlined mesh, fogged at the nearer
const char *kEdgeVertex = R"(#version 450
layout(location = 0) out vec2 uv;
void main() {
  vec2 corner = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  uv = vec2(corner.x, 1.0 - corner.y); // (the viewport's flipped)
  gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char *kEdgeFragment = R"(#version 450
layout(set = 0, binding = 0) uniform sampler2D colourImage;
layout(set = 0, binding = 1) uniform sampler2D surfaces;
layout(push_constant) uniform Edges {
  vec4 line, fogColour;
  vec2 texel;
  float pixels, fogDensity;
} edges;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 outColour;
void main() {
  vec4 here = texture(surfaces, uv);
  float distance = here.w == 0.0 ? 1e9 : abs(here.w), nearest = distance;
  bool edge = false;
  vec2 steps[4] = vec2[](vec2(1.0, 0.0), vec2(-1.0, 0.0), vec2(0.0, 1.0), vec2(0.0, -1.0));
  for (int i = 0; i < 4; ++i) {
    vec4 there = texture(surfaces, uv + steps[i] * edges.texel * edges.pixels);
    if (here.w <= 0.0 && there.w <= 0.0)
      continue; // (neither outlined)
    float thereDistance = there.w == 0.0 ? 1e9 : abs(there.w);
    bool step = abs(thereDistance - distance) > 0.05 * min(thereDistance, distance);
    bool crease = dot(here.xyz, there.xyz) < 0.85; // (about 30 degrees)
    if (step || crease) {
      edge = true;
      nearest = min(nearest, thereDistance);
    }
  }
  float fog = clamp(1.0 - exp(-edges.fogDensity * nearest), 0.0, 1.0);
  vec3 line = mix(edges.line.rgb, edges.fogColour.rgb, fog);
  outColour = vec4(edge ? line : texture(colourImage, uv).rgb, 1.0);
}
)";

const char *kOverlayVertex = R"(#version 450
layout(push_constant) uniform Screen { vec2 size; } screen; // in points
layout(location = 0) in vec2 point;
layout(location = 1) in vec4 colour;
layout(location = 0) out vec4 vColour;
void main() {
  gl_Position = vec4(point.x / screen.size.x * 2.0 - 1.0, 1.0 - point.y / screen.size.y * 2.0,
                     0.0, 1.0);
  vColour = colour;
}
)";

const char *kOverlayFragment = R"(#version 450
layout(location = 0) in vec4 vColour;
layout(location = 0) out vec4 outColour;
void main() { outColour = vColour; }
)";

// The shaders' Draw
struct DrawConstants {
  float model[16];
  float colour[4];
  float worldSnap, screenSnap;
  int lit;
  int toonBands;
  float extrude;
  int outlinePass;
  float outlined;
  int textured;
};

// The edge pass's push constants (the shader's Edges)
struct EdgeConstants {
  float line[4], fogColour[4];
  float texel[2];
  float pixels, fogDensity;
};
// Screen-space outlines' surfaces: a direction and a distance a pixel
constexpr VkFormat kSurfaceFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

// The scene's pipelines from `d` (its shaders and layout): solid and
// translucent, filled (both sides, or the back culled), outlines' hulls
// (fronts culled), wireframes where the device draws them, and lines; with
// screen-space outlines' second target if `d` has one (see-through meshes
// leaving it)
Renderer::Backend::ScenePipelines makeScenePipelines(Vulkan::PipelineDescription d) {
  Renderer::Backend::ScenePipelines p;
  p.solid = Vulkan::makePipeline(d);
  d.cullBack = true;
  p.solidCulled = Vulkan::makePipeline(d);
  d.cullBack = false;
  d.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  p.lines = Vulkan::makePipeline(d);
  d.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  d.blend = true;
  d.depthWrite = false;
  d.secondWritten = false;
  p.translucent = Vulkan::makePipeline(d);
  d.cullBack = true;
  p.translucentCulled = Vulkan::makePipeline(d);
  d.cullBack = false;
  d.cullFront = true;
  p.translucentHull = Vulkan::makePipeline(d);
  d.blend = false;
  d.depthWrite = true;
  d.secondWritten = true;
  p.solidHull = Vulkan::makePipeline(d);
  d.cullFront = false;
  if (Vulkan::wireframes()) {
    d.polygonMode = VK_POLYGON_MODE_LINE;
    p.solidWire = Vulkan::makePipeline(d);
    d.blend = true;
    d.depthWrite = false;
    d.secondWritten = false;
    p.translucentWire = Vulkan::makePipeline(d);
  }
  return p;
}

// The smallest buffer frameBytes makes
constexpr VkDeviceSize kFrameBufferSize = 256 * 1024;

glm::vec3 toGlm(const Vector3 &v) { return {v.x, v.y, v.z}; }
glm::vec3 toGlm(const Colour &c) { return glm::vec3(c.r, c.g, c.b) / 255.0f; }

glm::mat4 toGlm(const Transform &t) {
  glm::mat4 m(1.0f);
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 4; ++column)
      m[column][row] = t.m[row][column]; // glm is column-major, as GLSL is
  return m;
}

VkShaderModule compiled(Spirv::Stage stage, const char *glsl) {
  Spirv::Shader shader;
  std::string error;
  if (!Spirv::compile(stage, Spirv::Dialect::Vulkan, glsl, shader, error)) {
    std::fprintf(stderr, "Vulkan shader error: %s\n", error.c_str());
    return VK_NULL_HANDLE;
  }
  return Vulkan::makeShaderModule(shader.words);
}

VkDevice device() { return Vulkan::context()->device; }

void setColour(DrawConstants &draw, const Colour &colour, float alpha) {
  const glm::vec3 c = toGlm(colour);
  draw.colour[0] = c.r;
  draw.colour[1] = c.g;
  draw.colour[2] = c.b;
  draw.colour[3] = alpha;
}

} // namespace

// A texture's image and its set (set 1 of the scene's layout)
Renderer::Backend::LoadedTexture Renderer::Backend::makeLoadedTexture(int width, int height,
                                                                      const Colour *pixels) {
  LoadedTexture loaded;
  loaded.image = Vulkan::makeTexture(static_cast<std::uint32_t>(width),
                                     static_cast<std::uint32_t>(height), pixels);
  const VkDescriptorSetAllocateInfo allocate{.sType =
                                                 VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             .descriptorPool = texturePool,
                                             .descriptorSetCount = 1,
                                             .pSetLayouts = &textureLayout};
  if (vkAllocateDescriptorSets(device(), &allocate, &loaded.set) != VK_SUCCESS) {
    Vulkan::destroy(loaded.image);
    return {};
  }
  const VkDescriptorImageInfo info{textureSampler, loaded.image.view,
                                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                   .dstSet = loaded.set,
                                   .dstBinding = 0,
                                   .descriptorCount = 1,
                                   .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                   .pImageInfo = &info};
  vkUpdateDescriptorSets(device(), 1, &write, 0, nullptr);
  return loaded;
}

Renderer::Backend::Backend(Display &display)
    : display(display), surface(*static_cast<Vulkan::Surface *>(display.surface())) {
  sceneVertex = compiled(Spirv::Stage::Vertex, kSceneVertex);
  sceneFragment = compiled(Spirv::Stage::Fragment, kSceneFragment);
  // (the same, writing screen-space outlines' surfaces too)
  std::string surfaceSource = kSceneFragment;
  surfaceSource.insert(surfaceSource.find('\n') + 1, "#define WITH_SURFACE\n");
  sceneFragmentWithSurface = compiled(Spirv::Stage::Fragment, surfaceSource.c_str());
  edgeVertex = compiled(Spirv::Stage::Vertex, kEdgeVertex);
  edgeFragment = compiled(Spirv::Stage::Fragment, kEdgeFragment);
  overlayVertex = compiled(Spirv::Stage::Vertex, kOverlayVertex);
  overlayFragment = compiled(Spirv::Stage::Fragment, kOverlayFragment);

  // The scene's layout: Frame as set 0's uniform buffer, Draw pushed
  const VkDescriptorSetLayoutBinding frameBinding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT};
  const VkDescriptorSetLayoutCreateInfo setLayout{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &frameBinding};
  vkCreateDescriptorSetLayout(device(), &setLayout, nullptr, &frameLayout);
  // ... and the material's texture as set 1's sampled image
  const VkDescriptorSetLayoutBinding textureBinding{.binding = 0,
                                                    .descriptorType =
                                                        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    .descriptorCount = 1,
                                                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT};
  const VkDescriptorSetLayoutCreateInfo textureSetLayout{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &textureBinding};
  vkCreateDescriptorSetLayout(device(), &textureSetLayout, nullptr, &textureLayout);
  const VkDescriptorSetLayout sceneSets[2] = {frameLayout, textureLayout};
  const VkPushConstantRange drawRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                      sizeof(DrawConstants)};
  const VkPipelineLayoutCreateInfo scene{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                         .setLayoutCount = 2,
                                         .pSetLayouts = sceneSets,
                                         .pushConstantRangeCount = 1,
                                         .pPushConstantRanges = &drawRange};
  vkCreatePipelineLayout(device(), &scene, nullptr, &sceneLayout);
  const VkPushConstantRange screenRange{VK_SHADER_STAGE_VERTEX_BIT, 0, 2 * sizeof(float)};
  const VkPipelineLayoutCreateInfo overlay{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                           .pushConstantRangeCount = 1,
                                           .pPushConstantRanges = &screenRange};
  vkCreatePipelineLayout(device(), &overlay, nullptr, &overlayLayout);

  frameUniformBuffer =
      Vulkan::makeBuffer(sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
  const VkDescriptorPoolCreateInfo pool{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                        .maxSets = 1,
                                        .poolSizeCount = 1,
                                        .pPoolSizes = &poolSize};
  vkCreateDescriptorPool(device(), &pool, nullptr, &descriptors);
  const VkDescriptorSetAllocateInfo allocate{.sType =
                                                 VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             .descriptorPool = descriptors,
                                             .descriptorSetCount = 1,
                                             .pSetLayouts = &frameLayout};
  vkAllocateDescriptorSets(device(), &allocate, &frameSet);
  const VkDescriptorBufferInfo bufferInfo{frameUniformBuffer.buffer, 0, sizeof(FrameUniforms)};
  const VkWriteDescriptorSet write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                   .dstSet = frameSet,
                                   .dstBinding = 0,
                                   .descriptorCount = 1,
                                   .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                   .pBufferInfo = &bufferInfo};
  vkUpdateDescriptorSets(device(), 1, &write, 0, nullptr);
  // Textures' sets (one each, kMostTextures at most), their sampler, and
  // the blank one for materials without a texture
  const VkDescriptorPoolSize texturePoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                             kMostTextures + 1};
  const VkDescriptorPoolCreateInfo texturePoolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = kMostTextures + 1,
      .poolSizeCount = 1,
      .pPoolSizes = &texturePoolSize};
  vkCreateDescriptorPool(device(), &texturePoolInfo, nullptr, &texturePool);
  const VkSamplerCreateInfo smooth{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                   .magFilter = VK_FILTER_LINEAR,
                                   .minFilter = VK_FILTER_LINEAR,
                                   .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                   .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                   .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                   .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                   .maxLod = VK_LOD_CLAMP_NONE};
  vkCreateSampler(device(), &smooth, nullptr, &textureSampler);
  if (Vulkan::context()) {
    const Colour white;
    blank = makeLoadedTexture(1, 1, &white);
  }

  if (!sceneVertex || !sceneFragment || !overlayVertex || !overlayFragment)
    return;
  Vulkan::PipelineDescription d{.layout = sceneLayout,
                                .vertex = sceneVertex,
                                .fragment = sceneFragment,
                                .attributes = Vulkan::vertexAttributes(),
                                .stride = sizeof(Vertex),
                                .samples = Vulkan::frameSamples()};
  plain = makeScenePipelines(d);
  d.fragment = sceneFragmentWithSurface;
  d.secondFormat = kSurfaceFormat;
  withSurface = makeScenePipelines(d);
  makeEdgePipeline();

  static const VkVertexInputAttributeDescription kOverlayAttributes[] = {
      {0, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<std::uint32_t>(offsetof(Overlay::Vertex, x))},
      {1, 0, VK_FORMAT_R8G8B8A8_UNORM,
       static_cast<std::uint32_t>(offsetof(Overlay::Vertex, colour))}};
  overlayPipeline = Vulkan::makePipeline({.layout = overlayLayout,
                                          .vertex = overlayVertex,
                                          .fragment = overlayFragment,
                                          .attributes = kOverlayAttributes,
                                          .stride = sizeof(Overlay::Vertex),
                                          .blend = true,
                                          .depthTest = false,
                                          .depthWrite = false,
                                          .samples = Vulkan::frameSamples()});
}

Renderer::Backend::~Backend() {
  vkQueueWaitIdle(Vulkan::context()->queue);
  for (LoadedMesh &mesh : meshes) {
    Vulkan::destroy(mesh.vertices);
    Vulkan::destroy(mesh.indices);
    Vulkan::destroy(mesh.hullVertices);
  }
  for (Vulkan::Buffer &buffer : frameBuffers)
    Vulkan::destroy(buffer);
  Vulkan::destroy(frameUniformBuffer);
  for (const ScenePipelines &set : {plain, withSurface})
    for (VkPipeline pipeline : set.all())
      if (pipeline)
        vkDestroyPipeline(device(), pipeline, nullptr);
  for (VkPipeline pipeline : {overlayPipeline, edgePipeline})
    if (pipeline)
      vkDestroyPipeline(device(), pipeline, nullptr);
  for (Vulkan::Image *image : {&edgeColour, &surfaces, &surfaceSamples})
    Vulkan::destroy(*image);
  if (edgeSampler)
    vkDestroySampler(device(), edgeSampler, nullptr);
  if (edgeDescriptors)
    vkDestroyDescriptorPool(device(), edgeDescriptors, nullptr);
  if (edgeLayout)
    vkDestroyPipelineLayout(device(), edgeLayout, nullptr);
  if (edgeSetLayout)
    vkDestroyDescriptorSetLayout(device(), edgeSetLayout, nullptr);
  for (VkShaderModule module : {sceneFragmentWithSurface, edgeVertex, edgeFragment})
    if (module)
      vkDestroyShaderModule(device(), module, nullptr);
  for (VkShaderModule module : {sceneVertex, sceneFragment, overlayVertex, overlayFragment})
    if (module)
      vkDestroyShaderModule(device(), module, nullptr);
  for (LoadedTexture &texture : textures)
    Vulkan::destroy(texture.image);
  Vulkan::destroy(blank.image);
  if (texturePool)
    vkDestroyDescriptorPool(device(), texturePool, nullptr);
  if (textureSampler)
    vkDestroySampler(device(), textureSampler, nullptr);
  if (textureLayout)
    vkDestroyDescriptorSetLayout(device(), textureLayout, nullptr);
  vkDestroyDescriptorPool(device(), descriptors, nullptr);
  vkDestroyPipelineLayout(device(), sceneLayout, nullptr);
  vkDestroyPipelineLayout(device(), overlayLayout, nullptr);
  vkDestroyDescriptorSetLayout(device(), frameLayout, nullptr);
}

Renderer::Backend::Slice Renderer::Backend::frameBytes(const void *bytes, std::size_t length) {
  const VkDeviceSize size = (length + 15) / 16 * 16; // (each starts 16-byte aligned)
  while (frameBufferAt < frameBuffers.size() &&
         frameBufferUsed + size > frameBuffers[frameBufferAt].size) {
    ++frameBufferAt;
    frameBufferUsed = 0;
  }
  if (frameBufferAt == frameBuffers.size())
    frameBuffers.push_back(
        Vulkan::makeBuffer(std::max(kFrameBufferSize, size), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));
  Vulkan::Buffer &buffer = frameBuffers[frameBufferAt];
  std::memcpy(static_cast<unsigned char *>(buffer.mapped) + frameBufferUsed, bytes, length);
  const Slice slice{buffer.buffer, frameBufferUsed};
  frameBufferUsed += size;
  return slice;
}

void Renderer::Backend::drawNow(Mesh mesh, const Transform &where, const Material &material) {
  if (!recording || !plain.solid || !mesh || mesh.id > meshes.size() ||
      !meshes[mesh.id - 1].vertices.buffer)
    return;
  const LoadedMesh &loaded = meshes[mesh.id - 1];
  const bool seeThrough = material.alpha < 31 || loaded.translucent;
  // Back faces culled unless either side shows (a double-sided mesh or
  // material, or a wireframe, below)
  const bool cull = !loaded.doubleSided && !material.doubleSided;
  const ScenePipelines &p = scene();
  VkPipeline pipeline =
      seeThrough ? (cull ? p.translucentCulled : p.translucent) : (cull ? p.solidCulled : p.solid);
  if (material.wireframe && p.solidWire)
    pipeline = seeThrough ? p.translucentWire : p.solidWire;
  VkCommandBuffer commands = surface.commands;
  vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  DrawConstants draw{};
  const glm::mat4 model = toGlm(where);
  std::memcpy(draw.model, glm::value_ptr(model), sizeof(draw.model));
  setColour(draw, material.colour, static_cast<float>(std::min<int>(material.alpha, 31)) / 31.0f);
  draw.lit = material.shading == Material::Shading::Unlit ? 0 : 1;
  draw.toonBands =
      material.shading == Material::Shading::Toon ? toonBandsFor(material, environment) : 0;
  draw.outlined = material.outline ? 1.0f : 0.0f;
  draw.worldSnap = worldSnap;
  draw.screenSnap = screenSnap;
  // Its texture as set 1 (the blank one when it has none)
  VkDescriptorSet image = blank.set;
  if (material.texture && material.texture.id <= textures.size() &&
      textures[material.texture.id - 1].set) {
    image = textures[material.texture.id - 1].set;
    draw.textured = 1;
  }
  vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, sceneLayout, 1, 1, &image, 0,
                          nullptr);
  vkCmdPushConstants(commands, sceneLayout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(draw),
                     &draw);
  const VkDeviceSize start = 0;
  vkCmdBindVertexBuffers(commands, 0, 1, &loaded.vertices.buffer, &start);
  vkCmdBindIndexBuffer(commands, loaded.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(commands, loaded.indexCount, 1, 0, 0, 0);
  counting += loaded.cost;
  ++counting.draws;
  // The outline: the inverted hull, the mesh again pushed out along its
  // shared normals, front faces culled so only the rim shows, one colour
  if (material.outline && !material.wireframe && loaded.hullVertices.buffer &&
      environment.outline.style == Outline::Style::Hull) {
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      seeThrough ? p.translucentHull : p.solidHull);
    setColour(draw, material.outlineColour.value_or(environment.outline.colour), draw.colour[3]);
    draw.lit = 0;
    draw.outlinePass = 1;
    draw.extrude = environment.outline.width;
    vkCmdPushConstants(commands, sceneLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(draw),
                       &draw);
    vkCmdBindVertexBuffers(commands, 0, 1, &loaded.hullVertices.buffer, &start);
    vkCmdDrawIndexed(commands, loaded.indexCount, 1, 0, 0, 0);
    counting += loaded.cost;
    ++counting.draws;
  }
}

// The edge pass's pipeline: its images (the frame's colour and the
// surfaces) as a descriptor set, read with a nearest sampler; its uniforms
// pushed; no vertices
void Renderer::Backend::makeEdgePipeline() {
  if (!edgeVertex || !edgeFragment)
    return;
  const VkDescriptorSetLayoutBinding bindings[2] = {
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
  const VkDescriptorSetLayoutCreateInfo setLayout{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = bindings};
  vkCreateDescriptorSetLayout(device(), &setLayout, nullptr, &edgeSetLayout);
  const VkPushConstantRange constants{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(EdgeConstants)};
  const VkPipelineLayoutCreateInfo layout{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                          .setLayoutCount = 1,
                                          .pSetLayouts = &edgeSetLayout,
                                          .pushConstantRangeCount = 1,
                                          .pPushConstantRanges = &constants};
  vkCreatePipelineLayout(device(), &layout, nullptr, &edgeLayout);
  const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2};
  const VkDescriptorPoolCreateInfo pool{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                        .maxSets = 1,
                                        .poolSizeCount = 1,
                                        .pPoolSizes = &poolSize};
  vkCreateDescriptorPool(device(), &pool, nullptr, &edgeDescriptors);
  const VkDescriptorSetAllocateInfo allocate{.sType =
                                                 VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                             .descriptorPool = edgeDescriptors,
                                             .descriptorSetCount = 1,
                                             .pSetLayouts = &edgeSetLayout};
  vkAllocateDescriptorSets(device(), &allocate, &edgeSet);
  const VkSamplerCreateInfo sampler{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                    .magFilter = VK_FILTER_NEAREST,
                                    .minFilter = VK_FILTER_NEAREST,
                                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};
  vkCreateSampler(device(), &sampler, nullptr, &edgeSampler);
  edgePipeline = Vulkan::makePipeline({.layout = edgeLayout,
                                       .vertex = edgeVertex,
                                       .fragment = edgeFragment,
                                       .stride = 0,
                                       .depthTest = false,
                                       .depthWrite = false,
                                       .samples = Vulkan::frameSamples()});
}

// Screen-space outlines' images at the frame's size (the descriptor set
// pointed at them)
void Renderer::Backend::makeEdgeImages(int width, int height) {
  if (edgeColour.image && width == edgeWidth && height == edgeHeight)
    return;
  vkQueueWaitIdle(Vulkan::context()->queue); // (a frame being drawn may use them)
  for (Vulkan::Image *image : {&edgeColour, &surfaces, &surfaceSamples})
    Vulkan::destroy(*image);
  const auto w = static_cast<std::uint32_t>(width), h = static_cast<std::uint32_t>(height);
  const VkImageUsageFlags readable =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  edgeColour = Vulkan::makeImage(w, h, Vulkan::kColourFormat, VK_SAMPLE_COUNT_1_BIT, readable);
  surfaces = Vulkan::makeImage(w, h, kSurfaceFormat, VK_SAMPLE_COUNT_1_BIT, readable);
  if (Vulkan::frameSamples() != VK_SAMPLE_COUNT_1_BIT)
    surfaceSamples = Vulkan::makeImage(w, h, kSurfaceFormat, Vulkan::frameSamples(),
                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
  edgeWidth = width;
  edgeHeight = height;
  const VkDescriptorImageInfo images[2] = {
      {edgeSampler, edgeColour.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {edgeSampler, surfaces.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
  VkWriteDescriptorSet writes[2];
  for (std::uint32_t i = 0; i < 2; ++i)
    writes[i] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                 .dstSet = edgeSet,
                 .dstBinding = i,
                 .descriptorCount = 1,
                 .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                 .pImageInfo = &images[i]};
  vkUpdateDescriptorSets(device(), 2, writes, 0, nullptr);
}

// The edge pass: the scene's drawing ends, its images become readable, and
// a new one draws the frame with its outlines into the frame as usual (the
// overlay then goes over it)
void Renderer::Backend::drawEdges() {
  VkCommandBuffer commands = surface.commands;
  Vulkan::endRendering(commands);
  const Vulkan::Use drawn{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
  const Vulkan::Use read{VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT};
  Vulkan::transition(commands, edgeColour.image, Vulkan::kColourFormat, drawn, read);
  Vulkan::transition(commands, surfaces.image, kSurfaceFormat, drawn, read);
  const bool multisampled = surface.samples.image != VK_NULL_HANDLE;
  VkRenderingAttachmentInfo colour{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                   .imageView = surface.frame.view,
                                   .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  if (multisampled) {
    colour.imageView = surface.samples.view;
    colour.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colour.resolveImageView = surface.frame.view;
    colour.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colour.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
  VkRenderingAttachmentInfo depth{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                  .imageView = surface.depth.view,
                                  .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE};
  depth.clearValue.depthStencil = {1.0f, 0};
  const VkExtent2D extent{static_cast<std::uint32_t>(surface.width),
                          static_cast<std::uint32_t>(surface.height)};
  const VkRenderingInfo rendering{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea = {{0, 0}, extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = 1,
                                  .pColorAttachments = &colour,
                                  .pDepthAttachment = &depth};
  Vulkan::beginRendering(commands, rendering);
  const VkViewport viewport{0,
                            static_cast<float>(surface.height),
                            static_cast<float>(surface.width),
                            -static_cast<float>(surface.height),
                            0.0f,
                            1.0f};
  const VkRect2D scissor{{0, 0}, extent};
  vkCmdSetViewport(commands, 0, 1, &viewport);
  vkCmdSetScissor(commands, 0, 1, &scissor);
  vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, edgePipeline);
  vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, edgeLayout, 0, 1, &edgeSet, 0,
                          nullptr);
  EdgeConstants e{};
  const glm::vec3 line = toGlm(environment.outline.colour), fog = toGlm(environment.fog.colour);
  e.line[0] = line.r, e.line[1] = line.g, e.line[2] = line.b;
  e.fogColour[0] = fog.r, e.fogColour[1] = fog.g, e.fogColour[2] = fog.b;
  e.texel[0] = 1.0f / static_cast<float>(edgeWidth);
  e.texel[1] = 1.0f / static_cast<float>(edgeHeight);
  e.pixels = static_cast<float>(std::max<int>(environment.outline.pixels, 1));
  e.fogDensity = environment.fog.density;
  vkCmdPushConstants(commands, edgeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(e), &e);
  vkCmdDraw(commands, 3, 1, 0, 0);
}

void Renderer::Backend::drawOverlay(const Overlay &overlay) {
  // The shapes, then the text over them
  overlayTriangles.assign(overlay.triangles().begin(), overlay.triangles().end());
  for (const Overlay::Text &text : overlay.texts())
    appendTextTriangles(text, overlayTriangles);
  if (overlayTriangles.empty() || !overlayPipeline)
    return;
  VkCommandBuffer commands = surface.commands;
  vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipeline);
  const float screen[2] = {static_cast<float>(display.width()),
                           static_cast<float>(display.height())};
  vkCmdPushConstants(commands, overlayLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(screen),
                     screen);
  const Slice slice =
      frameBytes(overlayTriangles.data(), overlayTriangles.size() * sizeof(Overlay::Vertex));
  vkCmdBindVertexBuffers(commands, 0, 1, &slice.buffer, &slice.offset);
  vkCmdDraw(commands, static_cast<std::uint32_t>(overlayTriangles.size()), 1, 0, 0);
}

Renderer::Renderer(Display &display) : backend_(std::make_unique<Backend>(display)) {}

Renderer::~Renderer() = default;

Mesh Renderer::load(const MeshData &mesh) {
  Backend::LoadedMesh loaded;
  const std::vector<std::uint32_t> indices = triangleList(mesh);
  if (!mesh.vertices.empty() && !indices.empty()) {
    loaded.vertices = Vulkan::makeBuffer(mesh.vertices.size_bytes(),
                                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertices.data());
    loaded.indices = Vulkan::makeBuffer(indices.size() * sizeof(std::uint32_t),
                                        VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data());
    loaded.indexCount = static_cast<std::uint32_t>(indices.size());
    // Its outline's hull: the same indices, the normals shared (OutlineHull.hpp)
    const std::vector<Vertex> hull = outlineHullVertices(mesh.vertices);
    loaded.hullVertices = Vulkan::makeBuffer(hull.size() * sizeof(Vertex),
                                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, hull.data());
    loaded.translucent = hasTranslucentVertices(mesh);
    loaded.cost = costOf(mesh);
    loaded.bounds = boundsOf(mesh);
    loaded.doubleSided = mesh.doubleSided;
  }
  backend_->meshes.push_back(loaded);
  return Mesh{static_cast<std::uint32_t>(backend_->meshes.size())};
}

Texture Renderer::load(const TextureImage &image) {
  if (image.width <= 0 || image.height <= 0 ||
      backend_->textures.size() >= Backend::kMostTextures ||
      image.pixels.size() <
          static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height))
    return {};
  Backend::LoadedTexture loaded =
      backend_->makeLoadedTexture(image.width, image.height, image.pixels.data());
  if (!loaded.set)
    return {};
  backend_->textures.push_back(loaded);
  return {static_cast<std::uint32_t>(backend_->textures.size())};
}

void Renderer::unload(Texture texture) {
  if (!texture || texture.id > backend_->textures.size())
    return;
  Backend::LoadedTexture &loaded = backend_->textures[texture.id - 1];
  if (!loaded.set)
    return;
  vkQueueWaitIdle(Vulkan::context()->queue); // (a frame being drawn may use it)
  vkFreeDescriptorSets(device(), backend_->texturePool, 1, &loaded.set);
  Vulkan::destroy(loaded.image);
  loaded = {};
}

void Renderer::unload(Mesh mesh) {
  if (!mesh || mesh.id > backend_->meshes.size())
    return;
  Backend::LoadedMesh &loaded = backend_->meshes[mesh.id - 1];
  if (!loaded.vertices.buffer)
    return;
  // A frame still being drawn may use it
  vkQueueWaitIdle(Vulkan::context()->queue);
  Vulkan::destroy(loaded.vertices);
  Vulkan::destroy(loaded.indices);
  Vulkan::destroy(loaded.hullVertices);
  loaded = {};
}

void Renderer::beginFrame(const Camera &camera, const Environment &environment) {
  Backend &b = *backend_;
  Vulkan::Surface &s = b.surface;
  // One frame drawn at a time: the last one's buffers are reused once it's
  // done
  s.waitForLastFrame();
  b.frameBufferAt = 0;
  b.frameBufferUsed = 0;
  b.later.clear();
  b.counting = {};
  b.environment = environment;
  b.worldSnap = b.screenSnap = 0;
  overlay_.clear();

  const int width = b.display.pixelWidth(), height = b.display.pixelHeight();
  s.resize(width, height);

  b.eye = toGlm(camera.eye);
  const glm::mat4 view = glm::lookAt(b.eye, toGlm(camera.target), toGlm(camera.up));
  // Vulkan's depth goes from 0 to 1, not OpenGL's -1 to 1
  const glm::mat4 projection = glm::perspectiveRH_ZO(
      glm::radians(camera.fieldOfView), static_cast<float>(s.width) / static_cast<float>(s.height),
      camera.nearPlane, camera.farPlane);
  b.viewProj = projection * view;
  b.frustum.emplace(camera, static_cast<float>(s.width) / static_cast<float>(s.height));
  Backend::FrameUniforms f{};
  std::memcpy(f.viewProj, glm::value_ptr(b.viewProj), sizeof(f.viewProj));
  f.viewport[0] = static_cast<float>(s.width);
  f.viewport[1] = static_cast<float>(s.height);
  const glm::vec3 ambient = toGlm(environment.ambient);
  f.ambient[0] = ambient.r;
  f.ambient[1] = ambient.g;
  f.ambient[2] = ambient.b;
  for (unsigned i = 0; i < environment.lightCount; ++i) {
    const glm::vec3 direction = glm::normalize(toGlm(environment.lights[i].direction));
    const glm::vec3 lightColour = toGlm(environment.lights[i].colour);
    for (int c = 0; c < 3; ++c) {
      f.lightDirections[i][c] = direction[c];
      f.lightColours[i][c] = lightColour[c];
    }
  }
  f.lightCount = static_cast<int>(environment.lightCount);
  const glm::vec3 fog = toGlm(environment.fog.colour);
  f.fogColour[0] = fog.r;
  f.fogColour[1] = fog.g;
  f.fogColour[2] = fog.b;
  f.fogDensity = environment.fog.density;
  std::memcpy(b.frameUniformBuffer.mapped, &f, sizeof(f));

  const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                       .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
  vkResetCommandBuffer(s.commands, 0);
  vkBeginCommandBuffer(s.commands, &begin);
  const Vulkan::Use undefined{VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0};
  const Vulkan::Use colourTarget{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
  const bool multisampled = s.samples.image != VK_NULL_HANDLE;
  if (multisampled)
    Vulkan::transition(s.commands, s.samples.image, Vulkan::kColourFormat, undefined, colourTarget);
  Vulkan::transition(s.commands, s.frame.image, Vulkan::kColourFormat, undefined, colourTarget);
  // Screen-space outlines: the frame drawn into images of their own (with
  // the surfaces), the edges found in endFrame
  b.screenEdges = environment.outline.style == Outline::Style::ScreenSpace && b.edgePipeline;
  if (b.screenEdges) {
    b.makeEdgeImages(s.width, s.height);
    Vulkan::transition(s.commands, b.edgeColour.image, Vulkan::kColourFormat, undefined,
                       colourTarget);
    Vulkan::transition(s.commands, b.surfaces.image, kSurfaceFormat, undefined, colourTarget);
    if (multisampled)
      Vulkan::transition(s.commands, b.surfaceSamples.image, kSurfaceFormat, undefined,
                         colourTarget);
  }
  Vulkan::transition(
      s.commands, s.depth.image, Vulkan::kDepthFormat, undefined,
      {VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
       VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});

  const glm::vec3 background = toGlm(environment.backgroundSeen(camera.farPlane));
  VkRenderingAttachmentInfo colour{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                   .imageView = s.frame.view,
                                   .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                   .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  colour.clearValue.color = {{background.r, background.g, background.b, 1.0f}};
  if (multisampled) {
    // Drawn into the samples, resolved into the frame
    colour.imageView = s.samples.view;
    colour.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colour.resolveImageView = s.frame.view;
    colour.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colour.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }
  // (screen-space outlines: the frame into its own image, the surfaces too)
  VkRenderingAttachmentInfo surface{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                    .imageView = b.surfaces.view,
                                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE};
  surface.clearValue.color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // (no surface)
  if (b.screenEdges) {
    if (multisampled) {
      colour.resolveImageView = b.edgeColour.view;
      surface.imageView = b.surfaceSamples.view;
      surface.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
      surface.resolveImageView = b.surfaces.view;
      surface.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      surface.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    } else {
      colour.imageView = b.edgeColour.view;
    }
  }
  const VkRenderingAttachmentInfo colours[2] = {colour, surface};
  VkRenderingAttachmentInfo depth{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                                  .imageView = s.depth.view,
                                  .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                  .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                  .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE};
  depth.clearValue.depthStencil = {1.0f, 0};
  const VkExtent2D extent{static_cast<std::uint32_t>(s.width),
                          static_cast<std::uint32_t>(s.height)};
  const VkRenderingInfo rendering{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                  .renderArea = {{0, 0}, extent},
                                  .layerCount = 1,
                                  .colorAttachmentCount = b.screenEdges ? 2u : 1u,
                                  .pColorAttachments = colours,
                                  .pDepthAttachment = &depth};
  Vulkan::beginRendering(s.commands, rendering);
  // Flipped (y from the bottom), so clip space's y is up as in OpenGL
  const VkViewport viewport{0,
                            static_cast<float>(s.height),
                            static_cast<float>(s.width),
                            -static_cast<float>(s.height),
                            0.0f,
                            1.0f};
  const VkRect2D scissor{{0, 0}, extent};
  vkCmdSetViewport(s.commands, 0, 1, &viewport);
  vkCmdSetScissor(s.commands, 0, 1, &scissor);
  const VkDescriptorSet sets[2] = {b.frameSet, b.blank.set}; // (lines aren't textured)
  vkCmdBindDescriptorSets(s.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, b.sceneLayout, 0, 2, sets, 0,
                          nullptr);
  b.recording = true;
}

void Renderer::draw(Mesh mesh, const Transform &where, const Material &material) {
  const auto &meshes = backend_->meshes;
  // Wholly out of view: not drawn (Culling.hpp)
  if (mesh && mesh.id <= meshes.size() && backend_->frustum &&
      backend_->frustum->outside(meshes[mesh.id - 1].bounds, where)) {
    ++backend_->counting.culled;
    return;
  }
  if (material.alpha < 31 ||
      (mesh && mesh.id <= meshes.size() && meshes[mesh.id - 1].translucent)) {
    const glm::vec3 position = toGlm(where.position());
    backend_->later.push_back({mesh, where, material, glm::length(position - backend_->eye)});
    return;
  }
  backend_->drawNow(mesh, where, material);
}

void Renderer::drawLines(std::span<const Vector3> ends, Colour colour) {
  Backend &b = *backend_;
  if (ends.size() < 2 || !b.recording || !b.scene().lines)
    return;
  std::vector<Vertex> vertices;
  vertices.reserve(ends.size());
  for (const Vector3 &end : ends)
    vertices.push_back({end, {0, 1, 0}, Colour{}});
  const std::size_t count = vertices.size() / 2 * 2;
  VkCommandBuffer commands = b.surface.commands;
  vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, b.scene().lines);
  DrawConstants draw{};
  const glm::mat4 identity(1.0f);
  std::memcpy(draw.model, glm::value_ptr(identity), sizeof(draw.model));
  setColour(draw, colour, colour.a / 255.0f);
  draw.worldSnap = b.worldSnap;
  draw.screenSnap = b.screenSnap;
  vkCmdPushConstants(commands, b.sceneLayout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(draw),
                     &draw);
  const Backend::Slice slice = b.frameBytes(vertices.data(), vertices.size() * sizeof(Vertex));
  vkCmdBindVertexBuffers(commands, 0, 1, &slice.buffer, &slice.offset);
  vkCmdDraw(commands, static_cast<std::uint32_t>(count), 1, 0, 0);
  b.counting.polygons += static_cast<unsigned>(count / 2);
  b.counting.vertices += static_cast<unsigned>(count / 2 * 3);
  ++b.counting.draws;
}

void Renderer::endFrame() {
  Backend &b = *backend_;
  Vulkan::Surface &s = b.surface;
  if (!b.recording)
    return;
  // Translucent meshes over the solid ones, farthest first, each blending
  // over what's behind it without hiding what's drawn after
  std::ranges::stable_sort(b.later, [](const Backend::Later &x, const Backend::Later &y) {
    return x.distance > y.distance;
  });
  for (const Backend::Later &item : b.later)
    b.drawNow(item.mesh, item.where, item.material);
  b.later.clear();
  if (b.screenEdges)
    b.drawEdges();
  b.drawOverlay(overlay_);
  overlay_.clear();
  Vulkan::endRendering(s.commands);
  b.recording = false;

  // The display's last drawings (ImGui) over the finished frame
  const VkExtent2D extent{static_cast<std::uint32_t>(s.width),
                          static_cast<std::uint32_t>(s.height)};
  s.current = {s.commands, s.frame.view, extent};
  s.inPresent = true;
  b.display.present();
  s.inPresent = false;

  // Shown: copied into the swapchain's next image, which is presented at
  // the screen's refresh. Hidden: copied where Desktop::readPixel reads it.
  const Vulkan::Use drawn{VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
  const Vulkan::Use copiedFrom{VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_ACCESS_TRANSFER_READ_BIT};
  Vulkan::transition(s.commands, s.frame.image, Vulkan::kColourFormat, drawn, copiedFrom);
  std::uint32_t image = 0;
  bool presenting = false;
  if (b.display.visible()) {
    if (s.readySwapchain()) {
      const VkResult acquired = vkAcquireNextImageKHR(device(), s.swapchain, UINT64_MAX, s.acquired,
                                                      VK_NULL_HANDLE, &image);
      presenting = acquired == VK_SUCCESS || acquired == VK_SUBOPTIMAL_KHR;
      if (!presenting)
        s.swapchainStale = true;
    }
    if (presenting) {
      const VkImage target = s.swapchainImages[image];
      // (the swapchain's format may not be the frame's: a blit converts)
      Vulkan::transition(s.commands, target, Vulkan::kColourFormat,
                         {VK_IMAGE_LAYOUT_UNDEFINED, VK_PIPELINE_STAGE_TRANSFER_BIT, 0},
                         {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT});
      const VkImageBlit blit{
          .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          .srcOffsets = {{0, 0, 0}, {s.width, s.height, 1}},
          .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
          .dstOffsets = {{0, 0, 0},
                         {static_cast<std::int32_t>(s.swapchainExtent.width),
                          static_cast<std::int32_t>(s.swapchainExtent.height), 1}}};
      vkCmdBlitImage(s.commands, s.frame.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
      Vulkan::transition(
          s.commands, target, Vulkan::kColourFormat,
          {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
           VK_ACCESS_TRANSFER_WRITE_BIT},
          {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0});
    }
  } else {
    const VkBufferImageCopy copy{.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                 .imageExtent = {extent.width, extent.height, 1}};
    vkCmdCopyImageToBuffer(s.commands, s.frame.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           s.readback.buffer, 1, &copy);
    const VkMemoryBarrier toHost{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                                 .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                 .dstAccessMask = VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(s.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0,
                         1, &toHost, 0, nullptr, 0, nullptr);
  }
  vkEndCommandBuffer(s.commands);

  const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const VkSubmitInfo submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                            .waitSemaphoreCount = presenting ? 1u : 0u,
                            .pWaitSemaphores = &s.acquired,
                            .pWaitDstStageMask = &waitStage,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &s.commands,
                            .signalSemaphoreCount = presenting ? 1u : 0u,
                            .pSignalSemaphores = presenting ? &s.rendered[image] : nullptr};
  vkQueueSubmit(Vulkan::context()->queue, 1, &submit, s.done);
  s.submitted = true;
  if (presenting) {
    const VkPresentInfoKHR present{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &s.rendered[image],
                                   .swapchainCount = 1,
                                   .pSwapchains = &s.swapchain,
                                   .pImageIndices = &image};
    const VkResult shown = vkQueuePresentKHR(Vulkan::context()->queue, &present);
    if (shown != VK_SUCCESS)
      s.swapchainStale = true; // (out of date, or suboptimal: made again next frame)
  }
  if (!b.display.visible())
    s.waitForLastFrame(); // so readPixel reads it
  b.last = std::exchange(b.counting, {});
}

bool Renderer::toScreen(const Vector3 &point, float &x, float &y) const {
  const glm::vec4 clip = backend_->viewProj * glm::vec4(toGlm(point), 1.0f);
  if (clip.w <= 0)
    return false;
  x = (clip.x / clip.w * 0.5f + 0.5f) * static_cast<float>(backend_->display.width());
  y = (0.5f - clip.y / clip.w * 0.5f) * static_cast<float>(backend_->display.height());
  return true;
}

Capabilities Renderer::capabilities() const { return {}; }

FrameCounts Renderer::lastFrame() const { return backend_->last; }

FrameTimes Renderer::lastFrameTimes() const { return {}; } // (not measured here)

} // namespace ScratchPadd::Scene
