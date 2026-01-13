#ifndef AUDIO_PROCESSOR_SERVICE_ASYNC_H
#define AUDIO_PROCESSOR_SERVICE_ASYNC_H

#include <grpcpp/grpcpp.h>
#include <grpcpp/server_context.h>
#include <memory>
#include <queue>
#include <thread>
#include <semaphore>
#include "audio_processor.grpc.pb.h"

// Forward declarations for FFmpeg types (avoid including headers directly)
struct AVFormatContext;
struct AVCodecContext;
struct AVFilterGraph;
struct AVFilterContext;
struct AVFrame;

// Async service implementation using the gRPC async pattern (CallData state machines)
class AudioProcessorAsync {
public:
    explicit AudioProcessorAsync(int maxConcurrency);
    ~AudioProcessorAsync();
    
    void Run(const std::string& server_address, int num_cq_threads);

private:
    int maxConcurrency_;
    std::counting_semaphore<1024> concurrencySem;
    
    // Base class for all async RPC call handlers (state machines)
    class CallData {
    public:
        explicit CallData(AudioProcessorAsync* svc, grpc::ServerCompletionQueue* cq);
        virtual ~CallData() = default;
        virtual void Proceed() = 0;
        
    protected:
        AudioProcessorAsync* svc_;
        grpc::ServerCompletionQueue* cq_;
        grpc::ServerContext ctx_;
        enum CallStatus { CREATE, PROCESS, WRITING, FINISH };
        CallStatus status_;
    };
    
    // ExtractAudio unary RPC handler
    class ExtractAudioCallData : public CallData {
    public:
        ExtractAudioCallData(AudioProcessorAsync* svc, grpc::ServerCompletionQueue* cq);
        void Proceed() override;
        
    private:
        soundboard::ExtractAudioRequest request_;
        soundboard::ExtractAudioResponse response_;
        grpc::ServerAsyncResponseWriter<soundboard::ExtractAudioResponse> responder_;
    };
    
    // ApplyEffectsStream server-streaming RPC handler
    class ApplyEffectsStreamCallData : public CallData {
    public:
        ApplyEffectsStreamCallData(AudioProcessorAsync* svc, grpc::ServerCompletionQueue* cq);
        void Proceed() override;
        
    private:
        soundboard::ApplyEffectsRequest request_;
        grpc::ServerAsyncWriter<soundboard::AudioChunk> writer_;
        bool streaming_started_;
        bool streaming_no_effects_;
        FILE* ffmpeg_pipe_;
        char ffmpeg_buffer_[65536];
        int32_t chunk_sequence_;
        
        // libavfilter streaming state
        AVFormatContext* streaming_in_fmt_;
        AVCodecContext* streaming_dec_ctx_;
        AVCodecContext* streaming_enc_ctx_;
        AVFilterGraph* streaming_graph_;
        AVFilterContext* streaming_src_ctx_;
        AVFilterContext* streaming_sink_ctx_;
        AVFrame* streaming_frame_;
        AVFrame* streaming_filtered_frame_;
        int streaming_audio_stream_idx_;
        std::string streaming_filter_desc_;
        
        // Flushing state
        bool decoder_flushed_;
        bool filter_flushed_;
        bool encoder_flushed_;
        int64_t streaming_pts_;
        
        void start_processing();
        void send_next_chunk();
        void finish_stream();
    };
    
    std::unique_ptr<grpc::Server> server_;
    std::unique_ptr<soundboard::AudioProcessor::AsyncService> service_;
    std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> cqs_;
};

#endif
