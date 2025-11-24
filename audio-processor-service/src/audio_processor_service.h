#ifndef AUDIO_PROCESSOR_SERVICE_H
#define AUDIO_PROCESSOR_SERVICE_H

#include <grpcpp/grpcpp.h>
#include "audio_processor.grpc.pb.h"

class AudioProcessorServiceImpl final : public soundboard::AudioProcessor::Service {
public:
    grpc::Status ExtractAudio(
        grpc::ServerContext* context,
        const soundboard::ExtractAudioRequest* request,
        soundboard::ExtractAudioResponse* response) override;
    
    grpc::Status GetAudioInfo(
        grpc::ServerContext* context,
        const soundboard::AudioInfoRequest* request,
        soundboard::AudioInfoResponse* response) override;
    
    grpc::Status ApplyEffects(
        grpc::ServerContext* context,
        const soundboard::ApplyEffectsRequest* request,
        soundboard::ApplyEffectsResponse* response) override;
};

#endif 