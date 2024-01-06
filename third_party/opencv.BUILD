load("@rules_cc//cc:defs.bzl", "cc_library", "objc_library")
load("@rules_foreign_cc//foreign_cc:defs.bzl","cmake")

filegroup(
    name = "sources",
    srcs = glob(["**"]),
    visibility = ["//simple_make:__subpackages__"],
)

# Shows a standard library
cmake(
    name = "opencv",
    lib_source = ":sources",
    # pass include/version123 to C/C++ provider as include directory
    # out_include_dir = "include/version123",
    # out_lib_dir = ["opencv/lib"],
    # working_directory = "build/cmake",
    visibility = ["//visibility:public"],
    # out_static_libs = ["opencv.a"],
    out_shared_libs = ["libopencv_core.dylib","libopencv_flann.dylib","libopencv_imgproc.dylib","libopencv_ml.dylib","libopencv_photo.dylib","libopencv_dnn.dylib","libopencv_features2d.dylib","libopencv_imgcodecs.dylib","libopencv_videoio.dylib","libopencv_objdetect.dylib","libopencv_highgui.dylib","libopencv_calib3d.dylib","libopencv_stitching.dylib","libopencv_video.dylib"],
    deps = ["@zlib","@eigen"],
    cache_entries = {"OPENCV_EXTRA_FLAGS":"-DOPENCV_DISABLE_EIGEN_TENSOR_SUPPORT=1","BUILD_ZLIB":"OFF"},
)