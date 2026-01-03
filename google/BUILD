cc_library(
    name = "apis_cc_proto",
    hdrs = [
        "api/annotations.pb.h",
        "api/http.pb.h"
    ],
    srcs = [
        "api/annotations.pb.cc",
        "api/http.pb.cc"
    ],
    deps = [
        "@grpc//:grpc++",
    ],
    visibility = ["//:__subpackages__"]
)