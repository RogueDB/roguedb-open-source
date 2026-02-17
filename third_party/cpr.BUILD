load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "cpr",
    hdrs = glob(["include/**/*.h"]),
    includes = ["include"],
    visibility = ["//visibility:public"],

    srcs = glob(["cpr/**/*.cpp"]),
    deps = [
        "@curl//:curl"
    ],
)
