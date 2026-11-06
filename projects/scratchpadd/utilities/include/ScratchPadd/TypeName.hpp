#pragma once

// Readable names for types, for padds' names and logging.

#include <cctype>
#include <cstdlib>
#include <memory>
#include <string>
#include <typeinfo>

#if !defined(_MSC_VER)
#include <cxxabi.h>
#endif

namespace ScratchPadd {

// The readable name for a type_info's name(), "ScratchPadd::Text" say.
// GCC and Clang give a mangled name, demangled here (abi::__cxa_demangle's
// memory is malloc's, freed here; if it fails, the mangled name). MSVC's
// ABI (clang-cl too) gives a readable one with "class " or "struct " before
// each class name, taken out here, and "`anonymous namespace'" for what GCC
// and Clang call "(anonymous namespace)", given as theirs.
inline std::string demangle(const char *mangled) {
#if defined(_MSC_VER)
  std::string name = mangled;
  const auto partOfName = [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  };
  for (const std::string keyword : {"class ", "struct ", "enum ", "union "}) {
    for (auto at = name.find(keyword); at != std::string::npos; at = name.find(keyword, at)) {
      if (at > 0 && partOfName(name[at - 1])) {
        ++at; // the end of a longer name ("Myclass "), not the keyword
      } else {
        name.erase(at, keyword.size());
      }
    }
  }
  // As GCC and Clang name it
  const std::string anonymous = "`anonymous namespace'";
  for (auto at = name.find(anonymous); at != std::string::npos; at = name.find(anonymous, at)) {
    name.replace(at, anonymous.size(), "(anonymous namespace)");
  }
  return name;
#else
  int status = 0;
  std::unique_ptr<char, void (*)(void *)> name(
      abi::__cxa_demangle(mangled, nullptr, nullptr, &status), std::free);
  return status == 0 && name ? std::string(name.get()) : std::string(mangled);
#endif
}

// T's readable name.
template <typename T> std::string typeName() { return demangle(typeid(T).name()); }

} // namespace ScratchPadd
