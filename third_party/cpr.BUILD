load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

# filegroup(
#     name = "srcs",
#     srcs = glob(
#         include = ["**"],
#         exclude = [
#             "bazel-*/**",
#             "test/data/**",  # Exclude problematic test data
#         ],
#     ),
#     visibility = ["//visibility:public"],
# )

# cmake(
#     name = "cpr",
#     cache_entries = {
#         "CPR_CURL_USE_LIBPSL" : "OFF",
#     },
#     tags = ["requires-network"],
#     includes = ["include/cpr"],
#     lib_source = ":srcs",
#     out_shared_libs = select({
#         "@platforms//os:macos": ["libcpr.dylib"],
#         "@platforms//os:windows": ["libcpr.dll"],
#         "//conditions:default": ["libcpr.so.1"],
#     }),
#     visibility = ["//visibility:public"],
# )

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
