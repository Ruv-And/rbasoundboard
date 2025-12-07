#include "audio_processor_service.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

grpc::Status AudioProcessorServiceImpl::ExtractAudio(
    grpc::ServerContext* context,
    const soundboard::ExtractAudioRequest* request,
    soundboard::ExtractAudioResponse* response) {
    
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

grpc::Status AudioProcessorServiceImpl::ApplyEffects(
    grpc::ServerContext* context,
    const soundboard::ApplyEffectsRequest* request,
    soundboard::ApplyEffectsResponse* response) {
    
    std::cout << "ApplyEffects called:" << std::endl;
    std::cout << "  Audio: " << request->audio_path() << std::endl;
    std::cout << "  Speed: " << request->speed_factor() << "x" << std::endl;
    std::cout << "  Pitch: " << request->pitch_semitones() << " semitones" << std::endl;
    
    // Mock response
    response->set_success(true);
    response->set_processed_audio_path(request->output_path());
    response->set_error_message("");
    
    return grpc::Status::OK;
}