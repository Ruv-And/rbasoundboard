cmake_minimum_required(VERSION 3.15)
project(audio-processor-service)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(absl REQUIRED)

# Try to find utf8_range (may or may not work depending on install)
find_package(utf8_range QUIET)

# Proto files (use absolute path relative to this CMakeLists so builds from repo root work)
set(PROTO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/proto/audio_processor.proto)

# Manually find utf8_range libraries (since find_package may not provide proper targets)
file(GLOB UTF8_RANGE_LIBS
    "/usr/local/lib/libutf8_range*.a"
    "/usr/local/lib/libutf8_validity*.a"
    "/usr/lib/libutf8_range*.a"
    "/usr/lib/libutf8_validity*.a"
)

# Filter out duplicate utf8_range_lib.a if both libutf8_range.a and libutf8_range_lib.a exist
# Keep only the main libutf8_range.a and libutf8_validity.a
set(FILTERED_UTF8_RANGE_LIBS)
foreach(lib ${UTF8_RANGE_LIBS})
    if(NOT lib MATCHES "libutf8_range_lib\\.a$")
        list(APPEND FILTERED_UTF8_RANGE_LIBS ${lib})
    endif()
endforeach()

if(FILTERED_UTF8_RANGE_LIBS)
    set(UTF8_RANGE_LIBS ${FILTERED_UTF8_RANGE_LIBS})
    message(STATUS "Found utf8_range libs: ${UTF8_RANGE_LIBS}")
else()
    message(WARNING "No utf8_range libs found - linking may fail")
endif()

# Manually find Abseil static libs
file(GLOB ABSL_STATIC_LIBS
    "/usr/local/lib/libabsl*.a"
    "/usr/lib/libabsl*.a"
)
if(ABSL_STATIC_LIBS)
    message(STATUS "Found absl static libs (count: ${CMAKE_MATCH_COUNT})")
else()
    message(STATUS "No absl static libs found in /usr/local/lib or /usr/lib")
endif()

# Generate gRPC code
add_library(audio_proto ${PROTO_FILES})
target_link_libraries(audio_proto
    PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
    absl::log
    absl::strings
    absl::synchronization
    absl::status
)

# Link utf8_range and Abseil static libs if found
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

# Link libraries with proper ordering - utf8_range must come after protobuf
target_link_libraries(audio_server PRIVATE
    audio_proto
    gRPC::grpc++
)

# Force inclusion of all symbols from utf8_range static libraries
if(UTF8_RANGE_LIBS)
    target_link_libraries(audio_server PRIVATE 
        -Wl,--whole-archive
        ${UTF8_RANGE_LIBS}
        -Wl,--no-whole-archive
    )
endif()

# Link Abseil static libs
if(ABSL_STATIC_LIBS)
    target_link_libraries(audio_server PRIVATE ${ABSL_STATIC_LIBS})
endif()

target_include_directories(audio_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)