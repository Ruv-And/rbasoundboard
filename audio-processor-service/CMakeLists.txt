cmake_minimum_required(VERSION 3.15)
project(audio-processor-service)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Proto files
set(PROTO_FILES ../proto/audio_processor.proto)

# Generate gRPC code
add_library(audio_proto ${PROTO_FILES})
target_link_libraries(audio_proto
    PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)

target_include_directories(audio_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# Generate protobuf code
protobuf_generate(TARGET audio_proto LANGUAGE cpp)

# Generate gRPC code
protobuf_generate(
    TARGET audio_proto 
    LANGUAGE grpc 
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc 
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

# Main executable
add_executable(audio_server 
    src/main.cpp
    src/audio_processor_service.cpp
)

target_link_libraries(audio_server
    audio_proto
    gRPC::grpc++
)

target_include_directories(audio_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)