cc_library(
    name = "imgui_backend_sdl2",
    srcs = ["src/backend/imgui_impl_sdl2.cpp"],
    hdrs = ["include/backend/imgui_impl_sdl2.h",],
    visibility = ["//visibility:public"],
    strip_include_prefix = "include/backend",
    deps = ["@imgui//:imgui","@sdl2"],
)

cc_binary(
    name = "exampleImguiSDL2",
    srcs = ["src/main.cpp"],
    deps = ["@imgui//:imgui_backend_opengl3",":imgui_backend_sdl2"],
)

