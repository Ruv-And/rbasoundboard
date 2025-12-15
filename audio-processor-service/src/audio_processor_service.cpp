#include "audio_processor_service.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <algorithm>
#include <memory>

using namespace std::chrono_literals;

AudioProcessorServiceImpl::AudioProcessorServiceImpl(int maxConcurrency)
        : concurrencySem(std::clamp(maxConcurrency, 1, 1024)),
            maxConcurrency_{std::clamp(maxConcurrency, 1, 1024)} {}

grpc::Status AudioProcessorServiceImpl::ExtractAudio(
    grpc::ServerContext* context,
    const soundboard::ExtractAudioRequest* request,
    soundboard::ExtractAudioResponse* response) {
    // Try to acquire a concurrency permit with a short timeout
    if (!concurrencySem.try_acquire_for(100ms)) {
        std::cerr << "  BUSY: Concurrency limit reached for ExtractAudio" << std::endl;
        response->set_success(false);
        response->set_error_message("Processor busy, please retry");
        return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Processor busy");
    }
    auto release_guard = std::unique_ptr<void, void(*)(void*)>{
        reinterpret_cast<void*>(1),
        [this](void*) { this->concurrencySem.release(); }
    };

    std::cout << "ExtractAudio called:" << std::endl;
    std::cout << "  Video: " << request->video_path() << std::endl;
    std::cout << "  Output: " << request->output_path() << std::endl;
    std::cout << "  Format: " << request->format() << std::endl;
    std::cout << "  Bitrate: " << request->bitrate_kbps() << "kbps" << std::endl;
    
    // Build FFmpeg command
    std::string ffmpeg_cmd = "ffmpeg -i \"" + request->video_path() + "\" ";
    ffmpeg_cmd += "-vn "; // No video
    ffmpeg_cmd += "-acodec libmp3lame "; // MP3 codec
    ffmpeg_cmd += "-ab " + std::to_string(request->bitrate_kbps()) + "k "; // Bitrate
    ffmpeg_cmd += "-ar 44100 "; // Sample rate
    ffmpeg_cmd += "-y "; // Overwrite output file
    ffmpeg_cmd += "\"" + request->output_path() + "\" ";
    ffmpeg_cmd += "2>&1"; // Redirect stderr to stdout
    
    std::cout << "  Command: " << ffmpeg_cmd << std::endl;
    
    // Execute FFmpeg
    FILE* pipe = popen(ffmpeg_cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "  ERROR: Failed to run FFmpeg" << std::endl;
        response->set_success(false);
        response->set_error_message("Failed to execute FFmpeg command");
        return grpc::Status::OK;
    }
    
    // Read FFmpeg output
    char buffer[256];
    std::string ffmpeg_output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        ffmpeg_output += buffer;
    }
    
    int exit_code = pclose(pipe);
    
    if (exit_code != 0) {
        std::cerr << "  ERROR: FFmpeg failed with exit code " << exit_code << std::endl;
        std::cerr << "  FFmpeg output: " << ffmpeg_output << std::endl;
        response->set_success(false);
        response->set_error_message("FFmpeg processing failed: " + ffmpeg_output.substr(0, 200));
        return grpc::Status::OK;
    }
    
    // Get file size
    std::ifstream file(request->output_path(), std::ios::binary | std::ios::ate);
    int64_t file_size = 0;
    if (file.is_open()) {
        file_size = file.tellg();
        file.close();
    }
    
    response->set_success(true);
    response->set_audio_path(request->output_path());
    response->set_duration_seconds(0.0f); // Duration not used
    response->set_file_size_bytes(file_size);
    response->set_error_message("");
    
    std::cout << "  Result: SUCCESS" << std::endl;
    std::cout << "  File size: " << file_size << " bytes" << std::endl;
    
    return grpc::Status::OK;
}

grpc::Status AudioProcessorServiceImpl::GetAudioInfo(
    grpc::ServerContext* context,
    const soundboard::AudioInfoRequest* request,
    soundboard::AudioInfoResponse* response) {
    
    std::cout << "GetAudioInfo called for: " << request->audio_path() << std::endl;
    
    // Mock response
    response->set_duration_seconds(10.5f);
    response->set_sample_rate(44100);
    response->set_channels(2);
    response->set_bitrate_kbps(192);
    response->set_format("mp3");
    
    return grpc::Status::OK;
}

grpc::Status AudioProcessorServiceImpl::ApplyEffectsStream(
    grpc::ServerContext* context,
    const soundboard::ApplyEffectsRequest* request,
    grpc::ServerWriter<soundboard::AudioChunk>* writer) {
    // Try to acquire a concurrency permit with a short timeout
    if (!concurrencySem.try_acquire_for(100ms)) {
        std::cerr << "  BUSY: Concurrency limit reached for ApplyEffectsStream" << std::endl;
        return grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Processor busy");
    }
    auto release_guard = std::unique_ptr<void, void(*)(void*)>{
        reinterpret_cast<void*>(1),
        [this](void*) { this->concurrencySem.release(); }
    };

    std::cout << "ApplyEffectsStream called:" << std::endl;
    std::cout << "  Audio: " << request->audio_path() << std::endl;
    std::cout << "  Speed: " << request->speed_factor() << "x" << std::endl;
    std::cout << "  Pitch: " << request->pitch_factor() << "x" << std::endl;
    
    float speed = request->speed_factor();
    float pitch = request->pitch_factor();
    
    // Clamp values to safe range
    if (speed < 0.5f) speed = 0.5f;
    if (speed > 2.0f) speed = 2.0f;
    if (pitch < 0.5f) pitch = 0.5f;
    if (pitch > 2.0f) pitch = 2.0f;
    
    // Build FFmpeg filter chain
    std::string filter;
    
    if (speed != 1.0f && pitch != 1.0f) {
        // Both speed and pitch - use rubberband for pitch, atempo for speed
        char buf[256];
        snprintf(buf, sizeof(buf), "atempo=%.2f,rubberband=pitch=%.2f", speed, pitch);
        filter = buf;
    } else if (speed != 1.0f) {
        // Speed only - use atempo which preserves pitch
        char buf[64];
        snprintf(buf, sizeof(buf), "atempo=%.2f", speed);
        filter = buf;
    } else if (pitch != 1.0f) {
        // Pitch only - use rubberband for independent pitch shifting
        char buf[128];
        snprintf(buf, sizeof(buf), "rubberband=pitch=%.2f", pitch);
        filter = buf;
    } else {
        // No modifications - stream original file
        std::ifstream file(request->audio_path(), std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "  ERROR: Failed to open input file" << std::endl;
            return grpc::Status(grpc::StatusCode::NOT_FOUND, "Input file not found");
        }
        
        char buffer[65536];
        int32_t sequence = 0;
        while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
            soundboard::AudioChunk chunk;
            chunk.set_data(buffer, file.gcount());
            chunk.set_sequence_number(sequence++);
            
            if (!writer->Write(chunk)) {
                std::cerr << "  ERROR: Failed to write chunk " << sequence - 1 << std::endl;
                return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to stream data");
            }
        }
        
        file.close();
        std::cout << "  Result: SUCCESS (streamed " << sequence << " chunks)" << std::endl;
        return grpc::Status::OK;
    }
    
    // Build FFmpeg command to pipe output to stdout
    std::string ffmpeg_cmd = "ffmpeg -i \"" + request->audio_path() + "\" ";
    ffmpeg_cmd += "-af \"" + filter + "\" ";
    ffmpeg_cmd += "-acodec libmp3lame ";
    ffmpeg_cmd += "-ab 192k ";
    ffmpeg_cmd += "-ar 44100 ";
    ffmpeg_cmd += "-f mp3 - ";  // Output to stdout
    ffmpeg_cmd += "2>/dev/null";  // Suppress FFmpeg logs
    
    std::cout << "  Command: ffmpeg ... (piping to stdout)" << std::endl;
    
    // Execute FFmpeg with pipe
    FILE* pipe = popen(ffmpeg_cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "  ERROR: Failed to run FFmpeg" << std::endl;
        return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to execute FFmpeg");
    }
    
    // Stream chunks from FFmpeg output
    char buffer[65536];
    int32_t sequence = 0;
    size_t bytes_read;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        soundboard::AudioChunk chunk;
        chunk.set_data(buffer, bytes_read);
        chunk.set_sequence_number(sequence++);
        
        if (!writer->Write(chunk)) {
            std::cerr << "  ERROR: Failed to write chunk " << sequence - 1 << std::endl;
            pclose(pipe);
            return grpc::Status(grpc::StatusCode::INTERNAL, "Failed to stream data");
        }
    }
    
    int exit_code = pclose(pipe);
    
    if (exit_code != 0) {
        std::cerr << "  ERROR: FFmpeg failed with exit code " << exit_code << std::endl;
        return grpc::Status(grpc::StatusCode::INTERNAL, "FFmpeg processing failed");
    }
    
    std::cout << "  Result: SUCCESS (streamed " << sequence << " chunks)" << std::endl;
    
    return grpc::Status::OK;
}