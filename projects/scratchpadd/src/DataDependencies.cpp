#include <ScratchPadd/DataDependencies.hpp>
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <spdlog/spdlog.h>

#pragma clang diagnostic ignored "-Wambiguous-reversed-operator"
#include <filesystem>

#include "tools/cpp/runfiles/runfiles.h"
using bazel::tools::cpp::runfiles::Runfiles;

std::unique_ptr<Runfiles> runfiles;

// TODO: make this only runnable once with an assert
void ScratchPadd::Data::ConfigureDependencies(const std::string &processName) {
  std::string error;
  runfiles.reset(
  Runfiles::Create(processName, &error));
  if (runfiles == nullptr) {
    spdlog::info("error {}",error);
    exit(1);  // error handling
  }
  // Setting this global so that the GL_View can find the shaders
}

std::optional<std::string> ScratchPadd::Data::GetFullDependencyPath(const std::string &dependencyPath) {
    std::string path = runfiles->Rlocation(dependencyPath);
    if (!std::filesystem::exists(path)) {
        spdlog::info("Path does not exist: {}",path);
        for (auto const& dir_entry : std::filesystem::recursive_directory_iterator("/private/var/tmp/_bazel_jamesgrantham")) {
          spdlog::info("entry: {}",dir_entry.path().string());
        }
        return std::nullopt;
    }
    spdlog::info("GetFullDependencyPath: {}",path);
    return path;
}
