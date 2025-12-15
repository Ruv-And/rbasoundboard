#ifndef AUDIO_PROCESSOR_SERVICE_H
#define AUDIO_PROCESSOR_SERVICE_H

#include <grpcpp/grpcpp.h>
#include <semaphore>
#include "audio_processor.grpc.pb.h"

class AudioProcessorServiceImpl final : public soundboard::AudioProcessor::Service {
public:
    explicit AudioProcessorServiceImpl(int maxConcurrency);
    grpc::Status ExtractAudio(
        grpc::ServerContext* context,
        const soundboard::ExtractAudioRequest* request,
        soundboard::ExtractAudioResponse* response) override;
    
    grpc::Status GetAudioInfo(
        grpc::ServerContext* context,
        const soundboard::AudioInfoRequest* request,
        soundboard::AudioInfoResponse* response) override;
    
    grpc::Status ApplyEffectsStream(
        grpc::ServerContext* context,
        const soundboard::ApplyEffectsRequest* request,
        grpc::ServerWriter<soundboard::AudioChunk>* writer) override;

private:
    // Upper bound 1024 concurrent permits; actual permits set at runtime to maxConcurrency (capped to 1024)
    std::counting_semaphore<1024> concurrencySem{1};
    int maxConcurrency_{1};
};

#endif