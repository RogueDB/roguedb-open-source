# gazelle:ignore
# gazelle:exclude grpc
# gazelle:exclude rest

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_binary")

load("@rules_dotnet//dotnet:defs.bzl", "csharp_binary")
load("@rules_python//python:py_binary.bzl", "py_binary")
load("@rules_go//go:def.bzl", "go_binary")
load("@rules_java//java:defs.bzl", "java_binary")
load("@aspect_rules_js//js:defs.bzl", "js_binary")

load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_proto_grpc_java//:defs.bzl", "java_grpc_library")
load("@rules_proto_grpc_js//:defs.bzl", "js_grpc_library")
load("@rules_proto_grpc_csharp//:defs.bzl", "csharp_grpc_library")

load("@python_dependencies//:requirements.bzl", "requirement")

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

# csharp_grpc_library(
#     name = "roguedb_csharp_grpc",
#     protos = [":roguedb_proto"],
#     visibility = ["//visibility:public"],
# )

#csharp_binary(
#    name = "rest_sharp",
#    srcs = [
#        "rest/rest.cs"],
#    target_frameworks = ["net10.0"],
#    visibility = ["//visibility:public"],
#    deps = [
#    ],
#)

#csharp_binary(
#    name = "grpc_csharp",
#    srcs = [
#        "grpc/grpc.cs"],
#    target_frameworks = ["net10.0"], 
#    visibility = ["//visibility:public"],
#    deps = [
#        "@paket.main//google.protobuf",
#    ],
#)

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

proto_library(
    name = "roguedb_proto",
    srcs = ["roguedb.proto"],
    deps = [
        "@protobuf//:any_proto",
        "@googleapis//google/api:annotations_proto", 
    ],
)

java_grpc_library(
    name = "roguedb_java_grpc",
    protos = [":roguedb_proto"],
    deps = [
        "@maven//:com_google_api_grpc_proto_google_common_protos"
    ]
)

java_binary(
    name = "grpc_java",
    srcs = ["grpc/grpc.java"],
    main_class = "grpc.Main",
    deps = [
        "@maven//:com_fasterxml_jackson_core_jackson_databind",
        "@maven//:com_auth0_java_jwt",
        ":roguedb_java_grpc",
    ]
)

java_binary(
    name = "rest_java",
    srcs = ["rest/rest.java"],
    main_class = "rest.Main",
    deps = [
        "@maven//:com_fasterxml_jackson_core_jackson_databind",
        "@maven//:com_fasterxml_jackson_core_jackson_core",
        "@maven//:com_auth0_java_jwt",
    ]
)

js_grpc_library(
    name = "roguedb_js_grpc",
    protos = [":roguedb_proto"],
    visibility = ["//visibility:public"],
)

js_binary(
    name = "grpc_js",
    entry_point = "grpc/grpc.js",
    visibility = ["//visibility:public"],
    data = [
        "//:node_modules/@grpc/grpc-js",
        "//:node_modules/google-protobuf",
        ":roguedb_js_grpc",
    ],
)

js_binary(
    name = "rest_js",
    entry_point = "rest/rest.js",
    visibility = ["//visibility:public"],
    data = [
        "//:node_modules/axios",
        "//:node_modules/jsonwebtoken",
    ],
)

py_binary(
    name = "grpc_py",
    srcs = [
        "grpc/grpc.py",
        "roguedb_pb2.py",
        "roguedb_pb2_grpc.py"
    ],
    main = "grpc/grpc.py",
    visibility = ["//visibility:public"],
    deps = [
        requirement("PyJWT"),
        requirement("grpcio"),
        requirement("grpcio-status"),
        requirement("protobuf"),
        requirement("cryptography")
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
        requirement("cryptography")
    ],
)
