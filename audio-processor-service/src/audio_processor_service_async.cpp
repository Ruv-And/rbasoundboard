#include "audio_processor_service_async.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <functional>
#include <grpcpp/security/server_credentials.h>

using namespace std::chrono_literals;

// ============================================================================
// AudioProcessorAsync Implementation
// ============================================================================

AudioProcessorAsync::AudioProcessorAsync(int maxConcurrency)
    : maxConcurrency_(std::clamp(maxConcurrency, 1, 1024)),
      concurrencySem(std::clamp(maxConcurrency, 1, 1024)) {}

AudioProcessorAsync::~AudioProcessorAsync() {
    if (server_) {
        server_->Shutdown();
    }
}

void AudioProcessorAsync::Run(const std::string& server_address, int num_cq_threads) {
    service_ = std::make_unique<soundboard::AudioProcessor::AsyncService>();
    
    grpc::ServerBuilder builder;
    
    // Configure credentials
    std::shared_ptr<grpc::ServerCredentials> creds;
    const char* certPath = std::getenv("GRPC_SERVER_CERT_PATH");
    const char* keyPath = std::getenv("GRPC_SERVER_KEY_PATH");
    const char* rootCertPath = std::getenv("GRPC_SERVER_ROOT_CERT_PATH");
    
    if (certPath && keyPath) {
        std::cout << "Loading SSL/TLS credentials for secure gRPC..." << std::endl;
        grpc::SslServerCredentialsOptions ssl_opts;
        
        // Load server certificate and private key
        grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;
        std::ifstream cert_file(certPath);
        std::ifstream key_file(keyPath);
        
        if (!cert_file || !key_file) {
            std::cerr << "ERROR: Failed to open SSL certificate or key files" << std::endl;
            throw std::runtime_error("SSL configuration error");
        }
        
        key_cert_pair.cert_chain = std::string((std::istreambuf_iterator<char>(cert_file)),
                                               std::istreambuf_iterator<char>());
        key_cert_pair.private_key = std::string((std::istreambuf_iterator<char>(key_file)),
                                                std::istreambuf_iterator<char>());
        ssl_opts.pem_key_cert_pairs.push_back(key_cert_pair);
        
        // Optional: client certificate verification (mutual TLS)
        if (rootCertPath) {
            std::ifstream root_cert_file(rootCertPath);
            if (root_cert_file) {
                ssl_opts.pem_root_certs = std::string((std::istreambuf_iterator<char>(root_cert_file)),
                                                     std::istreambuf_iterator<char>());
                ssl_opts.client_certificate_request = GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;
                std::cout << "Mutual TLS enabled (client cert verification required)" << std::endl;
            }
        }
        
        creds = grpc::SslServerCredentials(ssl_opts);
        std::cout << "SSL/TLS credentials configured" << std::endl;
    } else {
        std::cout << "WARNING: Using insecure credentials (dev mode)" << std::endl;
        std::cout << "For production, set: GRPC_SERVER_CERT_PATH, GRPC_SERVER_KEY_PATH" << std::endl;
        creds = grpc::InsecureServerCredentials();
    }
    
    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(service_.get());
    
    // Create multiple completion queues (one per worker thread)
    for (int i = 0; i < num_cq_threads; i++) {
        cqs_.push_back(builder.AddCompletionQueue());
    }
    
    // Configure resource quota to limit threads
    grpc::ResourceQuota rq;
    rq.SetMaxThreads(maxConcurrency_ + 4);  // +4 for overhead threads
    builder.SetResourceQuota(rq);
    
    server_ = builder.BuildAndStart();
    std::cout << "========================================" << std::endl;
    std::cout << "Async Audio Processor Server listening on " << server_address << std::endl;
    std::cout << "Max concurrency: " << maxConcurrency_ << std::endl;
    std::cout << "Completion queues: " << num_cq_threads << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Spawn initial CallData for each method
    for (auto& cq : cqs_) {
        new ExtractAudioCallData(this, cq.get());
        new ApplyEffectsStreamCallData(this, cq.get());
    }
    
    // Run event loops in worker threads
    std::vector<std::thread> workers;
    for (size_t i = 0; i < cqs_.size(); i++) {
        workers.emplace_back([this, i]() {
            void* tag;
            bool ok;
            auto& cq = *cqs_[i];
            
            while (cq.Next(&tag, &ok)) {
                CallData* call = static_cast<CallData*>(tag);
                call->Proceed();
                // If Proceed() deletes call (final state), that's expected
                // New CallData instances will be spawned in FINISH state
            }
        });
    }
    
    // Wait for server shutdown
    server_->Wait();
    
    for (auto& cq : cqs_) {
        cq->Shutdown();
    }
    
    // Wait for worker threads
    for (auto& t : workers) {
        t.join();
    }
}

// ============================================================================
// CallData Base Class
// ============================================================================

AudioProcessorAsync::CallData::CallData(AudioProcessorAsync* svc, grpc::ServerCompletionQueue* cq)
    : svc_(svc), cq_(cq), status_(CREATE) {}

// ============================================================================
// ExtractAudioCallData Implementation
// ============================================================================

AudioProcessorAsync::ExtractAudioCallData::ExtractAudioCallData(
    AudioProcessorAsync* svc, grpc::ServerCompletionQueue* cq)
    : CallData(svc, cq), responder_(&ctx_) {
    Proceed();
}

void AudioProcessorAsync::ExtractAudioCallData::Proceed() {
    if (status_ == CREATE) {
        status_ = PROCESS;
        
        // Request the RPC (this enqueues a tag on the CQ)
        svc_->service_->RequestExtractAudio(&ctx_, &request_, &responder_, cq_, cq_, this);
    } else if (status_ == PROCESS) {
        // Spawn a new CallData immediately to handle the next incoming request
        new ExtractAudioCallData(svc_, cq_);
        
        status_ = FINISH;
        
        // Try to acquire concurrency permit
        if (!svc_->concurrencySem.try_acquire_for(100ms)) {
            std::cerr << "  BUSY: Concurrency limit reached for ExtractAudio" << std::endl;
            response_.set_success(false);
            response_.set_error_message("Processor busy, please retry");
            responder_.FinishWithError(
                grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Processor busy"),
                this);
            return;
        }
        
        auto release_guard = std::unique_ptr<void, std::function<void(void*)>>(
            reinterpret_cast<void*>(1),
            [this](void*) { this->svc_->concurrencySem.release(); }
        );
        
        std::cout << "ExtractAudio called:" << std::endl;
        std::cout << "  Video: " << request_.video_path() << std::endl;
        std::cout << "  Output: " << request_.output_path() << std::endl;
        std::cout << "  Format: " << request_.format() << std::endl;
        std::cout << "  Bitrate: " << request_.bitrate_kbps() << "kbps" << std::endl;
        
        // Build FFmpeg command
        std::string ffmpeg_cmd = "ffmpeg -i \"" + request_.video_path() + "\" ";
        ffmpeg_cmd += "-vn ";
        ffmpeg_cmd += "-acodec libmp3lame ";
        ffmpeg_cmd += "-ab " + std::to_string(request_.bitrate_kbps()) + "k ";
        ffmpeg_cmd += "-ar 44100 ";
        ffmpeg_cmd += "-y ";
        ffmpeg_cmd += "\"" + request_.output_path() + "\" ";
        ffmpeg_cmd += "2>&1";
        
        std::cout << "  Command: " << ffmpeg_cmd << std::endl;
        
        // Execute FFmpeg
        FILE* pipe = popen(ffmpeg_cmd.c_str(), "r");
        if (!pipe) {
            std::cerr << "  ERROR: Failed to run FFmpeg" << std::endl;
            response_.set_success(false);
            response_.set_error_message("Failed to execute FFmpeg command");
            responder_.Finish(response_, grpc::Status::OK, this);
            return;
        }
        
        char buffer[256];
        std::string ffmpeg_output;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            ffmpeg_output += buffer;
        }
        
        int exit_code = pclose(pipe);
        
        if (exit_code != 0) {
            std::cerr << "  ERROR: FFmpeg failed with exit code " << exit_code << std::endl;
            response_.set_success(false);
            response_.set_error_message("FFmpeg processing failed: " + ffmpeg_output.substr(0, 200));
            responder_.Finish(response_, grpc::Status::OK, this);
            return;
        }
        
        // Get file size
        std::ifstream file(request_.output_path(), std::ios::binary | std::ios::ate);
        int64_t file_size = 0;
        if (file.is_open()) {
            file_size = file.tellg();
            file.close();
        }
        
        response_.set_success(true);
        response_.set_audio_path(request_.output_path());
        response_.set_duration_seconds(0.0f);
        response_.set_file_size_bytes(file_size);
        response_.set_error_message("");
        
        std::cout << "  Result: SUCCESS" << std::endl;
        std::cout << "  File size: " << file_size << " bytes" << std::endl;
        
        responder_.Finish(response_, grpc::Status::OK, this);
    } else {  // FINISH
        delete this;
    }
}

// ============================================================================
// ApplyEffectsStreamCallData Implementation
// ============================================================================

AudioProcessorAsync::ApplyEffectsStreamCallData::ApplyEffectsStreamCallData(
    AudioProcessorAsync* svc, grpc::ServerCompletionQueue* cq)
    : CallData(svc, cq), writer_(&ctx_), streaming_started_(false), ffmpeg_pipe_(nullptr), chunk_sequence_(0) {
    Proceed();
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::Proceed() {
    if (status_ == CREATE) {
        status_ = PROCESS;
        
        // Request the RPC (this enqueues a tag on the CQ)
        svc_->service_->RequestApplyEffectsStream(&ctx_, &request_, &writer_, cq_, cq_, this);
    } else if (status_ == PROCESS) {
        // This state runs once: when a new RPC arrives
        // Spawn a new CallData immediately to handle the next incoming request
        new ApplyEffectsStreamCallData(svc_, cq_);
        
        // Try to acquire concurrency permit
        if (!svc_->concurrencySem.try_acquire_for(100ms)) {
            std::cerr << "  BUSY: Concurrency limit reached for ApplyEffectsStream" << std::endl;
            status_ = FINISH;
            writer_.Finish(grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Processor busy"), this);
            return;
        }
        
        std::cout << "ApplyEffectsStream called:" << std::endl;
        std::cout << "  Audio: " << request_.audio_path() << std::endl;
        std::cout << "  Speed: " << request_.speed_factor() << "x" << std::endl;
        std::cout << "  Pitch: " << request_.pitch_factor() << "x" << std::endl;
        
        start_processing();
        
        // Start streaming (will transition to WRITING state)
        send_next_chunk();
    } else if (status_ == WRITING) {
        // Previous Write() completed, send next chunk
        send_next_chunk();
    } else {  // FINISH
        svc_->concurrencySem.release();
        if (ffmpeg_pipe_) {
            pclose(ffmpeg_pipe_);
            ffmpeg_pipe_ = nullptr;
        }
        delete this;
    }
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::start_processing() {
    float speed = request_.speed_factor();
    float pitch = request_.pitch_factor();
    
    if (speed < 0.5f) speed = 0.5f;
    if (speed > 2.0f) speed = 2.0f;
    if (pitch < 0.5f) pitch = 0.5f;
    if (pitch > 2.0f) pitch = 2.0f;
    
    std::string filter;
    
    if (speed != 1.0f && pitch != 1.0f) {
        char buf[256];
        snprintf(buf, sizeof(buf), "atempo=%.2f,rubberband=pitch=%.2f", speed, pitch);
        filter = buf;
    } else if (speed != 1.0f) {
        char buf[64];
        snprintf(buf, sizeof(buf), "atempo=%.2f", speed);
        filter = buf;
    } else if (pitch != 1.0f) {
        char buf[128];
        snprintf(buf, sizeof(buf), "rubberband=pitch=%.2f", pitch);
        filter = buf;
    }
    
    // If no modifications, stream original file
    if (filter.empty()) {
        std::cout << "  Streaming original file (no effects)" << std::endl;
        // Mark to read from file instead of FFmpeg
        return;
    }
    
    // Build FFmpeg command
    std::string ffmpeg_cmd = "ffmpeg -i \"" + request_.audio_path() + "\" ";
    ffmpeg_cmd += "-af \"" + filter + "\" ";
    ffmpeg_cmd += "-acodec libmp3lame ";
    ffmpeg_cmd += "-ab 192k ";
    ffmpeg_cmd += "-ar 44100 ";
    ffmpeg_cmd += "-f mp3 - ";
    ffmpeg_cmd += "2>/dev/null";
    
    std::cout << "  Command: ffmpeg ... (piping to stdout)" << std::endl;
    
    ffmpeg_pipe_ = popen(ffmpeg_cmd.c_str(), "r");
    if (!ffmpeg_pipe_) {
        std::cerr << "  ERROR: Failed to run FFmpeg" << std::endl;
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to execute FFmpeg"), this);
    }
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::send_next_chunk() {
    size_t bytes_read = fread(ffmpeg_buffer_, 1, sizeof(ffmpeg_buffer_), ffmpeg_pipe_);
    
    if (bytes_read > 0) {
        soundboard::AudioChunk chunk;
        chunk.set_data(ffmpeg_buffer_, bytes_read);
        chunk.set_sequence_number(chunk_sequence_++);
        
        // Transition to WRITING state and queue async Write
        // When Write completes, Proceed() will be called again with status_==WRITING
        status_ = WRITING;
        writer_.Write(chunk, this);
    } else {
        // End of stream
        finish_stream();
    }
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::finish_stream() {
    status_ = FINISH;
    std::cout << "  Result: SUCCESS (streamed " << chunk_sequence_ << " chunks)" << std::endl;
    writer_.Finish(grpc::Status::OK, this);
}
