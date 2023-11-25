#pragma once

#ifdef _WIN32
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#include <cxxabi.h>
#include <iostream>

// inline std::string className(const std::string &classMethod) {
//   std::cout << "FUNCTION: " << classMethod << std::endl;
//   size_t scopeResolutionOpIndex = classMethod.find("::");
//   if (scopeResolutionOpIndex == std::string::npos) {
//     return "::";
//   }
//   size_t classNameStartIndex =
//       classMethod.substr(0, scopeResolutionOpIndex).rfind(" ") + 1;
//   size_t classNameLength = scopeResolutionOpIndex - classNameStartIndex;

//   return classMethod.substr(classNameStartIndex, classNameLength);
// }

// #define __CLASS_NAME__ ([] { \
//     const char* name = __PRETTY_FUNCTION__; \
//     size_t start = 0; \
//     size_t end = 0; \
//     for (size_t i = 0; name[i]; i++) { \
//         if (name[i] == ':' && name[i + 1] != ':') { \
//             start = i + 1; \
//         } else if (name[i] == '(') { \
//             end = i; \
//             break; \
//         } \
//     } \
//     return std::string(name + start, end - start); \
// }())

// #define __CLASS_NAME__ className(__PRETTY_FUNCTION__)

#define DEMANGLE_TYPEID_NAME(x)                                                \
  abi::__cxa_demangle(typeid((x)).name(), NULL, NULL, NULL)

std::string TypeName(auto x) { return DEMANGLE_TYPEID_NAME(x); }

#define __CLASS_NAME__ DEMANGLE_TYPEID_NAME(*this)

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

// Helper function to check if two variants hold the same type
template <typename... Types>
bool VariantsHoldSameType(const std::variant<Types...> &v1,
                          const std::variant<Types...> &v2) {
  return v1.index() == v2.index();
}
