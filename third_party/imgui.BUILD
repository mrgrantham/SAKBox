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
    linkopts = ["-framework OpenGL", "-framework CoreFoundation"],
)

cc_binary(
    name = "imgui_glfw",
    srcs = ["backends/imgui_impl_opengl3.cpp","examples/example_glfw_opengl3/main.cpp"],
    deps = ["@glfw",
    ":imgui_backend_opengl3",":imgui_backend_glfw"],
    linkopts = ["-framework OpenGL", "-framework CoreFoundation"],
)


# brew install sdl2
# c++ `sdl2-config --cflags` -I .. -I ../.. -I ../../backends
#   main.cpp ../../backends/imgui_impl_sdl2.cpp ../../backends/imgui_impl_opengl3.cpp ../../imgui*.cpp
#   `sdl2-config --libs` -framework OpenGl -framework CoreFoundation