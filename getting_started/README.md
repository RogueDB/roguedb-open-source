# Overview

RogueDB utilizes gRPC as the communication framework and Protocol Buffers (Protobuf) as the data format for sending and receiving messages. To communicate, you will need to generate the gRPC and Protobuf files in your target progamming language. All the files required are in the root of this directory with the suffix `.proto`.

gRPC Docs: https://grpc.io/
Protobuf Docs: https://protobuf.dev/

## gRPC and Protobuf

The following commands are sufficient with the pre-requisites installed:
- Protobuf: `protoc --cpp_out=. --python_out=. --pyi_out=. $(find -iname "*.proto")`
- gRPC: `protoc --grpc_out=. --plugin=protoc-gen-grpc="which grpc_cpp_plugin_native" path/to/directory/roguedb.proto`

Note the first command generates all proto files in the current directory for Python and C++. The second command requires the relative path to `roguedb.proto` and the gRPC plugin to build C++ only files. In both cases, refer to the documentation to determine the right options and pre-requisites for your specific progamming language.
