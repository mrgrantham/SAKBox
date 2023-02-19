load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "cyclone",
    srcs = ["src/body.cpp","src/collide_coarse.cpp","src/collide_fine.cpp","src/contacts.cpp","src/core.cpp","src/fgen.cpp","src/joints.cpp","src/particle.cpp","src/pcontacts.cpp","src/pfgen.cpp","src/plinks.cpp","src/pworld.cpp","src/random.cpp","src/world.cpp"],
    hdrs = ["include/cyclone/body.h","include/cyclone/cyclone.h","include/cyclone/collide_coarse.h","include/cyclone/collide_fine.h","include/cyclone/contacts.h","include/cyclone/core.h","include/cyclone/fgen.h","include/cyclone/joints.h","include/cyclone/particle.h","include/cyclone/precision.h","include/cyclone/pcontacts.h","include/cyclone/pfgen.h","include/cyclone/plinks.h","include/cyclone/pworld.h","include/cyclone/random.h","include/cyclone/world.h"],
    copts = ["-O2", "-fPIC"],
    strip_include_prefix = "/include",
    linkopts = ["-framework GLUT", "-framework OpenGL", "-framework Cocoa"],
)

cc_library(
    name = "gl",
    hdrs = ["include/gl/glut.h"],
    strip_include_prefix = "/include",
)

cc_library(
    name = "timing",
    srcs = ["src/demos/timing.cpp"],
    hdrs = ["src/demos/timing.h"],
    copts = ["-O2", "-fPIC"],
)

cc_library(
    name = "app",
    srcs = ["src/demos/app.cpp"],
    hdrs = ["src/demos/app.h", "src/demos/ogl_headers.h"],
    defines = ["GL_DEPRECATION"],
    deps = [":gl", ":timing", ":cyclone"],
    strip_include_prefix = "/src",
)

cc_binary(
    name = "ballistic",
    srcs = ["src/demos/ballistic/ballistic.cpp", "src/demos/main.cpp"],
    deps = [
        ":app",
    ],
)