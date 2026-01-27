# gazelle:ignore
# gazelle:exclude grpc
# gazelle:exclude rest

load("@python_dependencies//:requirements.bzl", "requirement")
load("@rules_go//go:def.bzl", "go_binary")

cc_library(
    name = "proto_cpp",
    hdrs = [
        "roguedb.pb.h",
        "roguedb.grpc.pb.h",
    ],
    srcs = [
        "roguedb.pb.cc",
        "roguedb.grpc.pb.cc",
    ],
    deps = [
        "@grpc//:grpc++",
        "//google:apis_cc_proto"
    ]
)

cc_binary(
    name = "grpc_cpp",
    srcs = [
        "grpc/grpc.cpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@jwt//:jwt",
        ":proto_cpp",
    ]
)

cc_binary(
    name = "rest_cpp",
    srcs = [
        "rest/rest.cpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@cpr//:cpr",
        "@jwt//:jwt",
    ],
)

go_binary(
    name = "grpc_go",
    srcs = ["grpc/grpc.go"],
    importpath = "github.com/roguedb/roguedb-open-source/getting_started",
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_golang_jwt_jwt_v5//:go_default_library",
        "@org_golang_google_grpc//:grpc",
        "@org_golang_google_grpc//credentials",
        "@org_golang_google_grpc//metadata",
        "@org_golang_google_protobuf//types/known/anypb",
        "//roguedb/core_schemas:core_schemas"
    ],
)

go_binary(
    name = "rest_go",
    srcs = ["rest/rest.go"],
    importpath = "github.com/roguedb/roguedb-open-source/getting_started",
    visibility = ["//visibility:public"],
    deps = [
        "@com_github_golang_jwt_jwt_v5//:go_default_library"
    ]
)

py_binary(
    name = "grpc_py",
    srcs = [
        "grpc/grpc.py",
    ],
    main = "grpc/grpc.py",
    visibility = ["//visibility:public"],
    deps = [
        requirement("PyJWT"),
        requirement("grpcio"),
        requirement("protobuf"),
    ],
)

py_binary(
    name = "rest_py",
    srcs = [
        "rest/rest.py",
    ],
    main = "rest/rest.py",
    visibility = ["//visibility:public"],
    deps = [
        requirement("PyJWT"),
        requirement("requests"),
    ],
)
