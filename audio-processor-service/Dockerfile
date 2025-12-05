########################################
# Multi-stage build: builder -> runtime
########################################

### Builder stage: compile gRPC and the audio server
FROM ubuntu:22.04 AS builder
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git \
    ca-certificates \
    wget \
    unzip \
    # Protobuf dev
    libprotobuf-dev \
    protobuf-compiler \
    # FFmpeg dev libs (optional for linking)
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    && rm -rf /var/lib/apt/lists/*

# Build and install gRPC C++ from source (provides gRPCConfig.cmake)
RUN git clone -b 20230802.0 https://github.com/abseil/abseil-cpp /tmp/abseil && \
    cd /tmp/abseil && mkdir -p build && cd build && \
    cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && make install && ldconfig && rm -rf /tmp/abseil

# Build and install gRPC C++ from source (provides gRPCConfig.cmake)
RUN git clone -b v1.60.0 --recurse-submodules https://github.com/grpc/grpc /tmp/grpc && \
    cd /tmp/grpc && mkdir -p cmake/build && cd cmake/build && \
    cmake ../.. -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && make install && ldconfig && \
    rm -rf /tmp/grpc

# App source
WORKDIR /app
COPY proto /app/proto
COPY audio-processor-service/CMakeLists.txt /app/
COPY audio-processor-service/src /app/src

# Build the audio server
RUN mkdir -p build && cd build && cmake .. && make -j$(nproc)


### Runtime stage: small image with only runtime deps + binary
FROM debian:bookworm-slim AS runtime
ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies (ffmpeg + libs)
RUN apt-get update && apt-get install -y \
    libstdc++6 \
    libgcc-s1 \
    ffmpeg \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy binary and runtime libraries from builder
COPY --from=builder /app/build/audio_server /usr/local/bin/audio_server
COPY --from=builder /usr/local/lib /usr/local/lib

ENV LD_LIBRARY_PATH=/usr/local/lib

# Create temp directory for audio processing
RUN mkdir -p /tmp/audio

EXPOSE 50051

ENTRYPOINT ["/usr/local/bin/audio_server"]