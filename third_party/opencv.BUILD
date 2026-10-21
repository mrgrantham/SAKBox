load("@rules_cc//cc:defs.bzl", "cc_library", "objc_library")
load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

# Only the modules the examples use are built (BUILD_LIST); add one there and
# to _MODULES to use it.
_MODULES = ["core", "imgproc", "imgcodecs", "videoio"]  # videoio links imgcodecs

cmake(
    name = "opencv",
    lib_source = ":sources",
    visibility = ["//visibility:public"],
    # Headers install to include/opencv4/opencv2/..., so expose include/opencv4
    # and code can use the usual #include <opencv2/...>.
    out_include_dir = "include/opencv4",
    # Windows isn't wired up yet: it installs version-suffixed names
    # (opencv_core4120.dll/.lib) under x64/vc*/ instead of lib/.
    out_shared_libs = select({
        "@platforms//os:macos": ["libopencv_%s.dylib" % m for m in _MODULES],
        "//conditions:default": ["libopencv_%s.so" % m for m in _MODULES],
    }),
    deps = ["@eigen"],
    cache_entries = {
        "BUILD_LIST": ",".join(_MODULES),
        "OPENCV_EXTRA_FLAGS": "-DOPENCV_DISABLE_EIGEN_TENSOR_SUPPORT=1",
        # OpenCV defaults to C++11, but Eigen 5 needs C++14 (std::conditional_t
        # and friends); GCC's standard library hides them in C++11 mode.
        "CMAKE_CXX_STANDARD": "17",
        # Bundled zlib links statically; @zlib's libz.1.dylib isn't in runfiles.
        "BUILD_ZLIB": "ON",
        # The image formats' libraries too, from OpenCV's own copies. That's
        # OpenCV's default on macOS; on Linux it used whatever the system had
        # installed, and the system's libraries asked for a zlib that isn't
        # there (the bundled one is a CMake target), so imgcodecs didn't link.
        "BUILD_JPEG": "ON",
        "BUILD_PNG": "ON",
        "BUILD_TIFF": "ON",
        "BUILD_WEBP": "ON",
        "BUILD_OPENJPEG": "ON",
        "BUILD_OPENEXR": "ON",
        # Only the C++ libraries are used. The binding generators also fail to
        # configure because they can't read the Eigen version.
        "BUILD_JAVA": "OFF",
        "BUILD_opencv_java_bindings_generator": "OFF",
        "BUILD_opencv_js_bindings_generator": "OFF",
        "BUILD_opencv_objc_bindings_generator": "OFF",
        "BUILD_opencv_python_bindings_generator": "OFF",
        "BUILD_opencv_python3": "OFF",
        "BUILD_TESTS": "OFF",
        "BUILD_PERF_TESTS": "OFF",
        "BUILD_opencv_apps": "OFF",
        # Give the libraries unversioned names (libopencv_core.dylib/.so, not
        # libopencv_core.412.dylib/.so.412) to match out_shared_libs.
        "CMAKE_PLATFORM_NO_VERSIONED_SONAME": "ON",
    } | select({
        # Bazel's Linux toolchain gives CMake gcc, not g++, as the C++
        # compiler, and gcc doesn't link the C++ standard library itself.
        "@platforms//os:linux": {
            "CMAKE_SHARED_LINKER_FLAGS": "-lstdc++",
            "CMAKE_EXE_LINKER_FLAGS": "-lstdc++",
        },
        "//conditions:default": {},
    }),
)
