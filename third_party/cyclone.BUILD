load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
    name = "cyclonelib",
    srcs = ["src/body.cpp","src/collide_course.cpp","src/collide_fine.cpp","src/contacts.cpp","src/core.cpp","src/fgen.cpp","src/joints.cpp","src/particle.cpp","src/pcontacts.cpp","src/pfgen.cpp","src/plinks.cpp","src/pworld.cpp","src/random.cpp","src/world.cpp"],
    hdrs = ["include/body.h","include/collide_course.h","include/collide_fine.h","include/contacts.h","include/core.h","include/fgen.h","include/joints.h","include/particle.h","include/pcontacts.h","include/pfgen.h","include/plinks.h","include/pworld.h","include/random.h","include/world.h"],
    copt = ["-O2", "-fPIC"],
    linkopts = ["-framework GLUT", "-framework OpenGL", "-framework Cocoa"],
)

cc_binary(
    name = "ballistic",
    srcs = ["src/demos/ballistic.cpp"],
    deps = [
        ":cyclonelib",
    ],
)