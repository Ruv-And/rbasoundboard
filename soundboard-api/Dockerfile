# Multi-stage build with gRPC code generation

# Stage 1: Generate gRPC code
# Use Eclipse Temurin JDK 25 for builds. If an official Maven image with
# `eclipse-temurin-25` tag is not available on your registry, change this
# back to `eclipse-temurin-21` or build with the repo's Dockerfile using JDK21.
FROM maven:3.9-eclipse-temurin-25 AS proto-builder

WORKDIR /build

# Install protoc
RUN apt-get update && apt-get install -y protobuf-compiler && rm -rf /var/lib/apt/lists/*

# Copy proto files (expects `proto/` to be available in the build context)
# When building with Docker Compose from the repo root set the build context to '.'
# so this `COPY proto` will pick up the top-level `proto/` directory.
COPY proto /build/proto

# Generate Java protobuf code
RUN mkdir -p /build/generated/java && \
    protoc --proto_path=/build/proto \
           --java_out=/build/generated/java \
           /build/proto/audio_processor.proto

# Download grpc plugin
RUN curl -L https://repo1.maven.org/maven2/io/grpc/protoc-gen-grpc-java/1.60.0/protoc-gen-grpc-java-1.60.0-linux-x86_64.exe \
    -o /usr/local/bin/protoc-gen-grpc-java && \
    chmod +x /usr/local/bin/protoc-gen-grpc-java

# Generate gRPC Java code
RUN protoc --proto_path=/build/proto \
           --plugin=protoc-gen-grpc-java=/usr/local/bin/protoc-gen-grpc-java \
           --grpc-java_out=/build/generated/java \
           /build/proto/audio_processor.proto

# Stage 2: Build application
# Build stage using Maven on Temurin JDK 25
FROM maven:3.9-eclipse-temurin-25 AS builder

WORKDIR /build

# Copy generated proto files
COPY --from=proto-builder /build/generated/java /build/src/main/java

# Copy pom.xml and download dependencies
COPY pom.xml .
RUN mvn dependency:go-offline -B

# Copy application source
COPY src ./src

# Build the application
RUN mvn clean package -DskipTests
 
# Stage 3: Runtime
# Runtime stage using Temurin JRE 25
FROM eclipse-temurin:25-jre-jammy

WORKDIR /app

# Copy jar from builder
COPY --from=builder /build/target/*.jar app.jar

# Create directories
RUN mkdir -p /app/uploads /app/audio /app/thumbnails

EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=30s --retries=3 \
  CMD curl -f http://localhost:8080/actuator/health || exit 1

ENTRYPOINT ["java", "-jar", "app.jar"]