cmake_minimum_required(VERSION 3.15)
project(audio-processor-service)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl REQUIRED)

# Proto files (use absolute path relative to this CMakeLists so builds from repo root work)
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/audio_processor.proto)

# Generate gRPC code
add_library(audio_proto ${PROTO_FILES})
target_link_libraries(audio_proto
    PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)

# Link absl components that protobuf/gRPC expect (resolves Abseil symbols)
target_link_libraries(audio_proto
    PUBLIC
    absl::log
    absl::strings
    absl::synchronization
    absl::status
)

# If find_package(absl) didn't provide all necessary targets or Abseil was
# installed manually into /usr/local, glob the installed static libs and link
# them into the proto library and final executable so symbols are resolved.
file(GLOB ABSL_STATIC_LIBS
    "/usr/local/lib/libabsl*.a"
    "/usr/lib/libabsl*.a"
)
if(ABSL_STATIC_LIBS)
    message(STATUS "Found absl static libs: ${ABSL_STATIC_LIBS}")
    target_link_libraries(audio_proto PUBLIC ${ABSL_STATIC_LIBS})
else()
    message(STATUS "No absl static libs found in /usr/local/lib or /usr/lib")
endif()

target_include_directories(audio_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

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

target_link_libraries(audio_server PRIVATE
    audio_proto
    gRPC::grpc++
)

target_include_directories(audio_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

# Also link any found Abseil static libs into the final executable to ensure
# the linker resolves Abseil symbols referenced by libprotobuf/libaudio_proto.
if(ABSL_STATIC_LIBS)
    target_link_libraries(audio_server PRIVATE ${ABSL_STATIC_LIBS})
endif()