load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

# OpenGL as each platform links it: macOS frameworks, Linux's libGL
OPENGL_LINKOPTS = select({
    "@platforms//os:macos": ["-framework OpenGL", "-framework CoreFoundation"],
    "@platforms//os:linux": ["-lGL"],
    "//conditions:default": [],
})

cc_library(
    name = "imgui",
    srcs = glob(["*.cpp"]),
    hdrs = glob(["*.h"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "imgui_backend_glfw",
    srcs = glob(["backends/imgui_impl_glfw.cpp"]),
    hdrs = glob(["backends/imgui_impl_glfw.h"]),
    visibility = ["//visibility:public"],
    strip_include_prefix = "/backends",
    deps = [":imgui","@glfw"],
)

cc_library(
    name = "imgui_backend_opengl3",
    srcs = ["backends/imgui_impl_opengl3.cpp"],
    hdrs = ["backends/imgui_impl_opengl3.h","backends/imgui_impl_opengl3_loader.h"],
    visibility = ["//visibility:public"],
    strip_include_prefix = "/backends",
    deps = [":imgui",],
    linkopts = OPENGL_LINKOPTS,
)



cc_library(
    name = "imgui_backend_sdl3",
    srcs = ["backends/imgui_impl_sdl3.cpp"],
    hdrs = ["backends/imgui_impl_sdl3.h",],
    visibility = ["//visibility:public"],
    strip_include_prefix = "backends",
    deps = [":imgui","@sdl3"],
)

cc_binary(
    name = "imgui_sdl3",
    srcs = ["examples/example_sdl3_opengl3/main.cpp"],
    deps = ["@sdl3",
    ":imgui_backend_opengl3",":imgui_backend_sdl3"],
    linkopts = OPENGL_LINKOPTS,
)

cc_binary(
    name = "imgui_glfw",
    srcs = ["backends/imgui_impl_opengl3.cpp","examples/example_glfw_opengl3/main.cpp"],
    deps = ["@glfw",
    ":imgui_backend_opengl3",":imgui_backend_glfw"],
    linkopts = OPENGL_LINKOPTS,
)


# brew install sdl2
# c++ `sdl2-config --cflags` -I .. -I ../.. -I ../../backends
#   main.cpp ../../backends/imgui_impl_sdl2.cpp ../../backends/imgui_impl_opengl3.cpp ../../imgui*.cpp
#   `sdl2-config --libs` -framework OpenGl -framework CoreFoundation
# ImGui::InputText and friends for std::string (resizes as you type)
cc_library(
    name = "imgui_stdlib",
    srcs = ["misc/cpp/imgui_stdlib.cpp"],
    hdrs = ["misc/cpp/imgui_stdlib.h"],
    strip_include_prefix = "misc/cpp",
    visibility = ["//visibility:public"],
    deps = [":imgui"],
)

cc_library(
    name = "imgui_backend_sdl2",
    srcs = ["backends/imgui_impl_sdl2.cpp"],
    hdrs = ["backends/imgui_impl_sdl2.h"],
    visibility = ["//visibility:public"],
    strip_include_prefix = "/backends",
    deps = [":imgui", "@//third_party/sdl2_flat_includes"],
)
