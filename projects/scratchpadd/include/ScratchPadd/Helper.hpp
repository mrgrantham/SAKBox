#pragma once

#ifdef _WIN32
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#include <iostream>

inline std::string className(const std::string &classMethod) {
  size_t scopeResolutionOpIndex = classMethod.find("::");
  if (scopeResolutionOpIndex == std::string::npos) {
    return "::";
  }
  size_t classNameStartIndex =
      classMethod.substr(0, scopeResolutionOpIndex).rfind(" ") + 1;
  size_t classNameLength = scopeResolutionOpIndex - classNameStartIndex;

  return classMethod.substr(classNameStartIndex, classNameLength);
}

#define __CLASS_NAME__ className(__PRETTY_FUNCTION__)

template <class... Ts> struct VariantHandler : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
VariantHandler(Ts...) -> VariantHandler<Ts...>; // line not needed in C++20...

template <typename To, typename From>
std::unique_ptr<To> dynamic_unique_cast(std::unique_ptr<From> &&p) {
  if (To *cast = dynamic_cast<To *>(p.get())) {
    std::unique_ptr<To> result(cast, std::move(p.get_deleter()));
    p.release();
    return result;
  }
  return std::unique_ptr<To>(nullptr); // or throw std::bad_cast() if you prefer
}

// Possible will work in newer compiler with full C++20 support
// template <typename To, typename From, typename Deleter>
// std::unique_ptr<To, Deleter> dynamic_unique_cast(std::unique_ptr<From,
// Deleter>&& p) {
//   if (To* cast = dynamic_cast<To*>(p.get())) {
//       std::unique_ptr<To, Deleter> result(cast, std::move(p.get_deleter()));
//       p.release();
//       return result;
//   }
//   return std::unique_ptr<To, Deleter>(nullptr); // or throw std::bad_cast()
//   if you prefer
// }

template <typename T> auto Make_Handler(T &handler) {
  return VariantHandler{
      handler, [&](auto &value) { std::cout << "Invalid Type" << std::endl; }};
}
