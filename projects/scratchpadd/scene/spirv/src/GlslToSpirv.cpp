#include <ScratchPadd/Scene/Spirv/GlslToSpirv.hpp>

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <spirv_cross.hpp>

#include <mutex>

namespace ScratchPadd::Scene::Spirv {

namespace {

// The loose uniforms' block's place and layout, found in the SPIR-V
std::optional<Shader::Uniforms> looseUniforms(const std::vector<std::uint32_t> &words) {
  const spirv_cross::Compiler reflection(words);
  for (const spirv_cross::Resource &block : reflection.get_shader_resources().uniform_buffers) {
    const spirv_cross::SPIRType &type = reflection.get_type(block.base_type_id);
    Shader::Uniforms uniforms;
    uniforms.set = reflection.get_decoration(block.id, spv::DecorationDescriptorSet);
    uniforms.binding = reflection.get_decoration(block.id, spv::DecorationBinding);
    uniforms.size = reflection.get_declared_struct_size(type);
    for (std::uint32_t member = 0; member < type.member_types.size(); ++member)
      uniforms.offsets[reflection.get_member_name(block.base_type_id, member)] =
          reflection.type_struct_member_offset(type, member);
    return uniforms; // the one block of loose uniforms
  }
  return std::nullopt;
}

} // namespace

bool compile(Stage stage, Dialect dialect, const std::string &glsl, Shader &out, std::string &error,
             std::uint32_t uniformBinding) {
  static std::once_flag started;
  std::call_once(started, [] { glslang::InitializeProcess(); });

  const EShLanguage language = stage == Stage::Vertex ? EShLangVertex : EShLangFragment;
  const bool opengl = dialect == Dialect::Opengl;
  glslang::TShader shader(language);
  const char *text = glsl.c_str();
  shader.setStrings(&text, 1);
  shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
  shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
  shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
  if (opengl) {
    shader.setEnvInputVulkanRulesRelaxed();
    shader.setGlobalUniformSet(0);
    shader.setGlobalUniformBinding(uniformBinding);
    shader.setAutoMapLocations(true);
    shader.setAutoMapBindings(true);
  }
  const auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
  if (!shader.parse(GetDefaultResources(), opengl ? 330 : 450, false, messages)) {
    error = shader.getInfoLog();
    return false;
  }
  glslang::TProgram program;
  program.addShader(&shader);
  if (!program.link(messages) || !program.mapIO()) {
    error = program.getInfoLog();
    return false;
  }
  out = {};
  glslang::GlslangToSpv(*program.getIntermediate(language), out.words);
  if (opengl) {
    try {
      out.uniforms = looseUniforms(out.words);
    } catch (const spirv_cross::CompilerError &e) {
      error = std::string("SPIRV-Cross: ") + e.what();
      return false;
    }
  }
  return true;
}

} // namespace ScratchPadd::Scene::Spirv
