#pragma once

#include <optional>
#include <string>
namespace DataDepRetriever {
void ConfigureDependencies(const std::string &processName);

std::optional<std::string>
GetFullDependencyPath(const std::string &dependencyPath);

} // namespace DataDepRetriever