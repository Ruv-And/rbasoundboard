# Use Ubuntu as base image
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies and runtime libraries
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    # gRPC and Protobuf
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    # FFmpeg for audio processing
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy proto files (shared between services)
COPY ../proto /app/proto

# Copy source code
COPY CMakeLists.txt /app/
COPY src /app/src

# Build the application
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Create temp directory for audio processing
RUN mkdir -p /tmp/audio

# Expose gRPC port
EXPOSE 50051

# Run the server
CMD ["./build/audio_server"]