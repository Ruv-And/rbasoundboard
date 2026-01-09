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
    # FFmpeg dev libs with rubberband support
    ffmpeg \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    libavfilter-dev \
    librubberband-dev \
    && rm -rf /var/lib/apt/lists/*

# Build and install Absei (dependency of protobuf)
RUN git clone -b 20230802.0 https://github.com/abseil/abseil-cpp /tmp/abseil && \
    cd /tmp/abseil && mkdir -p build && cd build && \
    cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && make install && ldconfig && rm -rf /tmp/abseil

# Build and install utf8_range (required by protobuf)
RUN git clone https://github.com/protocolbuffers/protobuf /tmp/protobuf-for-utf8 && \
    cd /tmp/protobuf-for-utf8 && git checkout v25.0 && git submodule update --init --recursive && \
    cd third_party/utf8_range && mkdir -p build && cd build && \
    cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dutf8_range_ENABLE_TESTS=OFF -Dutf8_range_ENABLE_INSTALL=ON && \
    make -j$(nproc) && make install && ldconfig && \
    rm -rf /tmp/protobuf-for-utf8

# Build and install protobuf separately with utf8_range support
RUN git clone -b v25.0 https://github.com/protocolbuffers/protobuf /tmp/protobuf && \
    cd /tmp/protobuf && git submodule update --init --recursive && \
    mkdir -p build && cd build && \
    cmake .. -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_EXAMPLES=OFF -Dutf8_range_ENABLE_TESTS=OFF && \
    make -j$(nproc) && make install && ldconfig && \
    rm -rf /tmp/protobuf

# Build and install gRPC C++ from source (provides gRPCConfig.cmake)
RUN git clone -b v1.60.0 --recurse-submodules https://github.com/grpc/grpc /tmp/grpc && \
    cd /tmp/grpc && mkdir -p cmake/build && cd cmake/build && \
    cmake ../.. -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
    -Dprotobuf_DIR=/usr/local/lib/cmake/protobuf && \
    make -j$(nproc) && make install && ldconfig && \
    rm -rf /tmp/grpc

# App source
WORKDIR /app
COPY proto /app/proto
COPY audio-processor-service/CMakeLists.txt /app/
COPY audio-processor-service/src /app/src

# Build the audio server  
# Note: Use --as-needed to avoid linking unnecessary libs, but first link objects
RUN mkdir -p build && cd build && cmake .. && make VERBOSE=1 audio_server 2>&1 | tee build.log; \
    if [ ! -f audio_server ]; then \
        echo "=== Build failed, checking FFmpeg libs ==="; \
        ldd /usr/lib/x86_64-linux-gnu/libavformat.so || true; \
        exit 1; \
    fi


### Runtime stage: small image with only runtime deps + binary
FROM ubuntu:22.04 AS runtime
ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies (FFmpeg 4.x libs matching what was built against)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    libgcc-s1 \
    libavformat58 \
    libavcodec58 \
    libavutil56 \
    libswresample3 \
    libavfilter7 \
    librubberband2 \
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