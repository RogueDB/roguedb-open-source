#!/bin/bash
protoc --cpp_out=. --go_out=. --python_out=. $(find -iname "*.proto")
protoc --csharp_out=getting_started/ getting_started/roguedb.proto
protoc --go-grpc_out=. getting_started/roguedb.proto

protoc --grpc_out=. --plugin=protoc-gen-grpc="/home/dev/grpc_plugins/grpc_cpp_plugin_native" getting_started/roguedb.proto
protoc --grpc_out=. --plugin=protoc-gen-grpc="/home/dev/grpc_plugins/grpc_python_plugin_native" getting_started/roguedb.proto