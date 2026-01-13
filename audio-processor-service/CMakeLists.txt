# Build order: proto generation -> audio_proto library -> audio_server executable
#
# Linking strategy (GNU ld processes left-to-right, single pass):
#   1. FFmpeg shared libs with --no-as-needed (prevents dropping "unused" .so files)
#   2. audio_proto static library (gRPC/protobuf generated code)
#   3. gRPC++ and transitive dependencies
#   4. utf8_range with --whole-archive (ensures all symbols exported)
#   5. Abseil static libraries
#   6. System libraries (m, pthread, dl)

cmake_minimum_required(VERSION 3.15)
project(audio-processor-service)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl REQUIRED)
find_package(utf8_range QUIET)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswresample libavfilter)

# --- Proto Files ---
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/audio_processor.proto)

# --- Locate Static Libraries ---
file(GLOB UTF8_RANGE_LIBS "/usr/local/lib/libutf8_range*.a" "/usr/local/lib/libutf8_validity*.a")
list(FILTER UTF8_RANGE_LIBS EXCLUDE REGEX "libutf8_range_lib\\.a$")

file(GLOB ABSL_STATIC_LIBS "/usr/local/lib/libabsl*.a")

# --- Proto Library ---
add_library(audio_proto ${PROTO_FILES})

target_link_libraries(audio_proto PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
    absl::log
    absl::strings
    absl::synchronization
    absl::status
)

if(UTF8_RANGE_LIBS)
    target_link_libraries(audio_proto PUBLIC ${UTF8_RANGE_LIBS})
endif()

if(ABSL_STATIC_LIBS)
    target_link_libraries(audio_proto PUBLIC ${ABSL_STATIC_LIBS})
endif()

target_include_directories(audio_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

protobuf_generate(TARGET audio_proto LANGUAGE cpp)
protobuf_generate(
    TARGET audio_proto 
    LANGUAGE grpc 
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc 
    PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
)

# --- Main Executable ---
add_executable(audio_server 
    src/main.cpp
    src/audio_processor_service_async.cpp
    src/audio_conversion.cpp
)

target_include_directories(audio_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
    ${FFMPEG_INCLUDE_DIRS}
)

# --- Linking (order matters for GNU ld) ---

# FFmpeg: linked first with --no-as-needed to ensure shared libs are kept
target_link_libraries(audio_server PRIVATE
    "-Wl,--no-as-needed"
    "-L${FFMPEG_LIBRARY_DIRS}"
    -lavformat -lavcodec -lavutil -lswresample -lavfilter
    "-Wl,--as-needed"
)

# gRPC/Protobuf
target_link_libraries(audio_server PRIVATE audio_proto gRPC::grpc++)

# utf8_range: use --whole-archive to include all symbols
if(UTF8_RANGE_LIBS)
    target_link_libraries(audio_server PRIVATE
        -Wl,--whole-archive ${UTF8_RANGE_LIBS} -Wl,--no-whole-archive
    )
endif()

# Abseil
if(ABSL_STATIC_LIBS)
    target_link_libraries(audio_server PRIVATE ${ABSL_STATIC_LIBS})
endif()

# System
target_link_libraries(audio_server PRIVATE m pthread dl)