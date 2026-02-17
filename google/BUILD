load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "apis_cc_proto",
    srcs = [
        "api/annotations.pb.cc",
        "api/http.pb.cc",
    ],
    hdrs = [
        "api/annotations.pb.h",
        "api/http.pb.h",
    ],
    visibility = ["//:__subpackages__"],
    deps = [
        "@grpc//:grpc++",
    ],
)