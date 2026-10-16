load("@rules_cc//cc:defs.bzl", "cc_library")

# Forwarding headers so `#include <SDL.h>` (SDL's own convention, used by
# Dear ImGui's SDL2 backend) works with our SDL2 build, which exposes
# <SDL2/SDL.h>.
cc_library(
    name = "sdl2_flat_includes",
    hdrs = glob(["*.h"]),
    includes = ["."],
    visibility = ["//visibility:public"],
    deps = ["@sdl2"],
)
