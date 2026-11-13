#include "GlslToMsl.hpp"

#include <ScratchPadd/Scene/Spirv/GlslToSpirv.hpp>

#include <spirv_msl.hpp>

#include <utility>

namespace ScratchPadd::Scene::Metal {

bool translate(Stage stage, const std::string &glsl, TranslatedShader &out, std::string &error) {
  Spirv::Shader spirv;
  if (!Spirv::compile(stage == Stage::Vertex ? Spirv::Stage::Vertex : Spirv::Stage::Fragment,
                      Spirv::Dialect::Opengl, glsl, spirv, error))
    return false;
  try {
    spirv_cross::CompilerMSL msl(std::move(spirv.words));
    spirv_cross::CompilerMSL::Options options = msl.get_msl_options();
    options.platform = spirv_cross::CompilerMSL::Options::macOS;
    options.set_msl_version(2, 1);
    msl.set_msl_options(options);
    spirv_cross::CompilerGLSL::Options common = msl.get_common_options();
    common.vertex.flip_vert_y = stage == Stage::Vertex;
    msl.set_common_options(common);
    out.source = msl.compile();
    const auto model =
        stage == Stage::Vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
    out.entryPoint = msl.get_cleansed_entry_point_name("main", model);

    // The loose uniforms' block, at the buffer index Metal gave it
    if (spirv.uniforms) {
      for (const spirv_cross::Resource &block : msl.get_shader_resources().uniform_buffers) {
        out.uniformBuffer = static_cast<int>(msl.get_automatic_msl_resource_binding(block.id));
        break;
      }
      out.uniformSize = spirv.uniforms->size;
      out.uniformOffsets = spirv.uniforms->offsets;
    }
  } catch (const spirv_cross::CompilerError &e) {
    error = std::string("SPIRV-Cross: ") + e.what();
    return false;
  }
  return true;
}

} // namespace ScratchPadd::Scene::Metal
