#include "GlslToMsl.hpp"

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <spirv_msl.hpp>

#include <mutex>
#include <vector>

namespace ScratchPadd::Scene::Metal {

bool translate(Stage stage, const std::string &glsl, TranslatedShader &out, std::string &error) {
  static std::once_flag started;
  std::call_once(started, [] { glslang::InitializeProcess(); });

  const EShLanguage language = stage == Stage::vertex ? EShLangVertex : EShLangFragment;
  glslang::TShader shader(language);
  const char *text = glsl.c_str();
  shader.setStrings(&text, 1);
  shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
  // An OpenGL shader's loose uniforms (iTime and so on) are gathered into a
  // block, and what has no location or binding is given one
  shader.setEnvInputVulkanRulesRelaxed();
  shader.setAutoMapLocations(true);
  shader.setAutoMapBindings(true);
  const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
  if (!shader.parse(GetDefaultResources(), 330, false, messages)) {
    error = shader.getInfoLog();
    return false;
  }
  glslang::TProgram program;
  program.addShader(&shader);
  if (!program.link(messages) || !program.mapIO()) {
    error = program.getInfoLog();
    return false;
  }
  std::vector<unsigned> spirv;
  glslang::GlslangToSpv(*program.getIntermediate(language), spirv);

  try {
    spirv_cross::CompilerMSL msl(std::move(spirv));
    spirv_cross::CompilerMSL::Options options = msl.get_msl_options();
    options.platform = spirv_cross::CompilerMSL::Options::macOS;
    options.set_msl_version(2, 1);
    msl.set_msl_options(options);
    spirv_cross::CompilerGLSL::Options common = msl.get_common_options();
    common.vertex.flip_vert_y = stage == Stage::vertex;
    msl.set_common_options(common);
    out.source = msl.compile();
    const auto model =
        stage == Stage::vertex ? spv::ExecutionModelVertex : spv::ExecutionModelFragment;
    out.entryPoint = msl.get_cleansed_entry_point_name("main", model);

    for (const spirv_cross::Resource &block : msl.get_shader_resources().uniform_buffers) {
      const spirv_cross::SPIRType &type = msl.get_type(block.base_type_id);
      out.uniformBuffer = static_cast<int>(msl.get_automatic_msl_resource_binding(block.id));
      out.uniformSize = msl.get_declared_struct_size(type);
      for (std::uint32_t member = 0; member < type.member_types.size(); ++member)
        out.uniformOffsets[msl.get_member_name(block.base_type_id, member)] =
            msl.type_struct_member_offset(type, member);
      break; // the one block of loose uniforms
    }
  } catch (const spirv_cross::CompilerError &e) {
    error = std::string("SPIRV-Cross: ") + e.what();
    return false;
  }
  return true;
}

} // namespace ScratchPadd::Scene::Metal
