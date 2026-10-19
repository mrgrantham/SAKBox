load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

# Dear ImGui's demo window on SDL2 + OpenGL 3, using the backends that ship
# with ImGui (@imgui//:imgui_backend_*).
cc_binary(
    name = "exampleImguiSDL2",
    srcs = ["src/main.cpp"],
    deps = [
        "@imgui//:imgui_backend_opengl3",
        "@imgui//:imgui_backend_sdl2",
        "@sdl2",
    ],
)
