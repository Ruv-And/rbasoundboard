#include "audio_processor_service_async.h"
#include "audio_conversion.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <functional>
#include <cinttypes>

// FFmpeg is a C library
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

#include <grpcpp/security/server_credentials.h>

using namespace std::chrono_literals;

// Helper to format FFmpeg error codes
static std::string av_err_to_string(int errnum)
{
    char buf[256];
    av_strerror(errnum, buf, sizeof(buf));
    return std::string(buf);
}

// ============================================================================
// AudioProcessorAsync Impl
// ============================================================================

AudioProcessorAsync::AudioProcessorAsync(int maxConcurrency)
    : maxConcurrency_(std::clamp(maxConcurrency, 1, 1024)),
      concurrencySem(std::clamp(maxConcurrency, 1, 1024)) {}

AudioProcessorAsync::~AudioProcessorAsync()
{
    if (server_)
    {
        server_->Shutdown();
    }
}

void AudioProcessorAsync::Run(const std::string &server_address, int num_cq_threads)
{
    service_ = std::make_unique<soundboard::AudioProcessor::AsyncService>();

    grpc::ServerBuilder builder;

    // Configure credentials
    std::shared_ptr<grpc::ServerCredentials> creds;
    const char *certPath = std::getenv("GRPC_SERVER_CERT_PATH");
    const char *keyPath = std::getenv("GRPC_SERVER_KEY_PATH");
    const char *rootCertPath = std::getenv("GRPC_SERVER_ROOT_CERT_PATH");

    if (certPath && keyPath)
    {
        std::cout << "Loading SSL/TLS credentials for secure gRPC..." << std::endl;
        grpc::SslServerCredentialsOptions ssl_opts;

        // Load server certificate and private key
        grpc::SslServerCredentialsOptions::PemKeyCertPair key_cert_pair;
        std::ifstream cert_file(certPath);
        std::ifstream key_file(keyPath);

        if (!cert_file || !key_file)
        {
            std::cerr << "ERROR: Failed to open SSL certificate or key files" << std::endl;
            throw std::runtime_error("SSL configuration error");
        }

        key_cert_pair.cert_chain = std::string((std::istreambuf_iterator<char>(cert_file)),
                                               std::istreambuf_iterator<char>());
        key_cert_pair.private_key = std::string((std::istreambuf_iterator<char>(key_file)),
                                                std::istreambuf_iterator<char>());
        ssl_opts.pem_key_cert_pairs.push_back(key_cert_pair);

        // Optional: client certificate verification (mutual TLS)
        if (rootCertPath)
        {
            std::ifstream root_cert_file(rootCertPath);
            if (root_cert_file)
            {
                ssl_opts.pem_root_certs = std::string((std::istreambuf_iterator<char>(root_cert_file)),
                                                      std::istreambuf_iterator<char>());
                ssl_opts.client_certificate_request = GRPC_SSL_REQUEST_AND_REQUIRE_CLIENT_CERTIFICATE_AND_VERIFY;
                std::cout << "Mutual TLS enabled (client cert verification required)" << std::endl;
            }
        }

        creds = grpc::SslServerCredentials(ssl_opts);
        std::cout << "SSL/TLS credentials configured" << std::endl;
    }
    else
    {
        std::cout << "WARNING: Using insecure credentials (dev mode)" << std::endl;
        std::cout << "For production, set: GRPC_SERVER_CERT_PATH, GRPC_SERVER_KEY_PATH" << std::endl;
        creds = grpc::InsecureServerCredentials();
    }

    builder.AddListeningPort(server_address, creds);
    builder.RegisterService(service_.get());

    // Create multiple completion queues (one per worker thread)
    for (int i = 0; i < num_cq_threads; i++)
    {
        cqs_.push_back(builder.AddCompletionQueue());
    }

    // Configure resource quota to limit threads
    grpc::ResourceQuota rq;
    rq.SetMaxThreads(maxConcurrency_ + 4); // +4 for overhead threads
    builder.SetResourceQuota(rq);

    server_ = builder.BuildAndStart();
    std::cout << "========================================" << std::endl;
    std::cout << "Async Audio Processor Server listening on " << server_address << std::endl;
    std::cout << "Max concurrency: " << maxConcurrency_ << std::endl;
    std::cout << "Completion queues: " << num_cq_threads << std::endl;
    std::cout << "========================================" << std::endl;

    // Spawn initial CallData for each method
    for (auto &cq : cqs_)
    {
        new ExtractAudioCallData(this, cq.get());
        new ApplyEffectsStreamCallData(this, cq.get());
    }

    // Run event loops in worker threads
    std::vector<std::thread> workers;
    for (size_t i = 0; i < cqs_.size(); i++)
    {
        workers.emplace_back([this, i]()
                             {
            void* tag;
            bool ok;
            auto& cq = *cqs_[i];
            
            while (cq.Next(&tag, &ok)) {
                CallData* call = static_cast<CallData*>(tag);
                call->Proceed();
                // If Proceed() deletes call (final state), that's expected
                // New CallData instances will be spawned in FINISH state
            } });
    }

    // Wait for server shutdown
    server_->Wait();

    for (auto &cq : cqs_)
    {
        cq->Shutdown();
    }

    // Wait for worker threads
    for (auto &t : workers)
    {
        t.join();
    }
}

// ============================================================================
// CallData Base Class
// ============================================================================

AudioProcessorAsync::CallData::CallData(AudioProcessorAsync *svc, grpc::ServerCompletionQueue *cq)
    : svc_(svc), cq_(cq), status_(CREATE) {}

// ============================================================================
// ExtractAudioCallData Implementation
// ============================================================================

AudioProcessorAsync::ExtractAudioCallData::ExtractAudioCallData(
    AudioProcessorAsync *svc, grpc::ServerCompletionQueue *cq)
    : CallData(svc, cq), responder_(&ctx_)
{
    Proceed();
}

void AudioProcessorAsync::ExtractAudioCallData::Proceed()
{
    if (status_ == CREATE)
    {
        status_ = PROCESS;

        // Request the RPC (this enqueues a tag on the CQ)
        svc_->service_->RequestExtractAudio(&ctx_, &request_, &responder_, cq_, cq_, this);
    }
    else if (status_ == PROCESS)
    {
        // Spawn a new CallData immediately to handle the next incoming request
        new ExtractAudioCallData(svc_, cq_);

        status_ = FINISH;

        // Try to acquire concurrency permit
        if (!svc_->concurrencySem.try_acquire_for(100ms))
        {
            std::cerr << "  BUSY: Concurrency limit reached for ExtractAudio" << std::endl;
            response_.set_success(false);
            response_.set_error_message("Processor busy, please retry");
            responder_.FinishWithError(
                grpc::Status(grpc::StatusCode::RESOURCE_EXHAUSTED, "Processor busy"),
                this);
            return;
        }

        auto release_guard = std::unique_ptr<void, std::function<void(void *)>>(
            reinterpret_cast<void *>(1),
            [this](void *)
            { this->svc_->concurrencySem.release(); });

        std::cout << "ExtractAudio called:" << std::endl;
        std::cout << "  Video: " << request_.video_path() << std::endl;
        std::cout << "  Output: " << request_.output_path() << std::endl;
        std::cout << "  Format: " << request_.format() << std::endl;
        std::cout << "  Bitrate: " << request_.bitrate_kbps() << "kbps" << std::endl;

        // Use libav* APIs instead of spawning ffmpeg process
        std::string libav_err;
        std::cout << "  Converting using libav (in-process)" << std::endl;
        if (!soundboard::convert_to_mp3_libav(request_.video_path(), request_.output_path(), request_.bitrate_kbps(), libav_err))
        {
            std::cerr << "  ERROR: libav conversion failed: " << libav_err << std::endl;
            response_.set_success(false);
            response_.set_error_message(std::string("FFmpeg processing failed: ") + libav_err.substr(0, 200));
            responder_.Finish(response_, grpc::Status::OK, this);
            return;
        }

        // Get file size
        std::ifstream file(request_.output_path(), std::ios::binary | std::ios::ate);
        int64_t file_size = 0;
        if (file.is_open())
        {
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
    }
    else
    { // FINISH
        delete this;
    }
}

// ============================================================================
// ApplyEffectsStreamCallData Impl
// ============================================================================

AudioProcessorAsync::ApplyEffectsStreamCallData::ApplyEffectsStreamCallData(
    AudioProcessorAsync *svc, grpc::ServerCompletionQueue *cq)
    : CallData(svc, cq), writer_(&ctx_), streaming_started_(false), streaming_no_effects_(false),
      ffmpeg_pipe_(nullptr), chunk_sequence_(0),
      streaming_in_fmt_(nullptr), streaming_dec_ctx_(nullptr), streaming_enc_ctx_(nullptr),
      streaming_graph_(nullptr), streaming_src_ctx_(nullptr), streaming_sink_ctx_(nullptr),
      streaming_frame_(nullptr), streaming_filtered_frame_(nullptr), streaming_audio_stream_idx_(-1),
      decoder_flushed_(false), filter_flushed_(false), encoder_flushed_(false), streaming_pts_(0)
{
    Proceed();
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::Proceed()
{
    if (status_ == CREATE)
    {
        status_ = PROCESS;

        // Request the RPC (enqueues a tag on the CQ)
        svc_->service_->RequestApplyEffectsStream(&ctx_, &request_, &writer_, cq_, cq_, this);
    }
    else if (status_ == PROCESS)
    {
        // This state runs once: when a new RPC arrives
        // Spawn a new CallData immediately to handle the next incoming request
        new ApplyEffectsStreamCallData(svc_, cq_);

        // Try to acquire concurrency permit
        if (!svc_->concurrencySem.try_acquire_for(100ms))
        {
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

        // Start streaming (transition to WRITING state)
        send_next_chunk();
    }
    else if (status_ == WRITING)
    {
        // Previous Write() completed, send next chunk
        send_next_chunk();
    }
    else
    { // FINISH
        svc_->concurrencySem.release();
        if (ffmpeg_pipe_)
        {
            pclose(ffmpeg_pipe_);
            ffmpeg_pipe_ = nullptr;
        }
        delete this;
    }
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::start_processing()
{
    float speed = request_.speed_factor();
    float pitch = request_.pitch_factor();

    if (speed < 0.5f)
        speed = 0.5f;
    if (speed > 2.0f)
        speed = 2.0f;
    if (pitch < 0.5f)
        pitch = 0.5f;
    if (pitch > 2.0f)
        pitch = 2.0f;

    // If no modifications, stream original file directly
    if (speed == 1.0f && pitch == 1.0f)
    {
        std::cout << "  Streaming original file (no effects)" << std::endl;
        streaming_no_effects_ = true;
        return;
    }

    streaming_no_effects_ = false;

    // Build libavfilter graph for time-stretching and pitch-shifting
    std::string filter_desc;
    if (speed != 1.0f && pitch != 1.0f)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "atempo=%.2f,rubberband=pitch=%.2f", speed, pitch);
        filter_desc = buf;
    }
    else if (speed != 1.0f)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "atempo=%.2f", speed);
        filter_desc = buf;
    }
    else if (pitch != 1.0f)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "rubberband=pitch=%.2f", pitch);
        filter_desc = buf;
    }

    std::cout << "  Using libavfilter: \"" << filter_desc << "\"" << std::endl;

    // Open input file
    AVFormatContext *in_fmt = nullptr;
    if (int ret = avformat_open_input(&in_fmt, request_.audio_path().c_str(), nullptr, nullptr))
    {
        std::cerr << "  ERROR: avformat_open_input: " << av_err_to_string(ret) << std::endl;
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to open input"), this);
        return;
    }

    if (int ret = avformat_find_stream_info(in_fmt, nullptr))
    {
        std::cerr << "  ERROR: avformat_find_stream_info: " << av_err_to_string(ret) << std::endl;
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to find stream info"), this);
        return;
    }

    int audio_stream_index = -1;
    for (unsigned i = 0; i < in_fmt->nb_streams; ++i)
    {
        if (in_fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_index = i;
            break;
        }
    }
    if (audio_stream_index < 0)
    {
        std::cerr << "  ERROR: no audio stream found" << std::endl;
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "No audio stream"), this);
        return;
    }

    // Decode input
    AVStream *in_stream = in_fmt->streams[audio_stream_index];
    const AVCodec *dec = avcodec_find_decoder(in_stream->codecpar->codec_id);
    if (!dec)
    {
        std::cerr << "  ERROR: decoder not found" << std::endl;
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Decoder not found"), this);
        return;
    }

    AVCodecContext *dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
    {
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to alloc decoder"), this);
        return;
    }
    avcodec_parameters_to_context(dec_ctx, in_stream->codecpar);
    if (int ret = avcodec_open2(dec_ctx, dec, nullptr))
    {
        std::cerr << "  ERROR: avcodec_open2: " << av_err_to_string(ret) << std::endl;
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to open decoder"), this);
        return;
    }

    // Setup encoder
    const AVCodec *enc = avcodec_find_encoder(AV_CODEC_ID_MP3);
    if (!enc)
    {
        std::cerr << "  ERROR: MP3 encoder not found" << std::endl;
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Encoder not found"), this);
        return;
    }

    AVCodecContext *enc_ctx = avcodec_alloc_context3(enc);
    if (!enc_ctx)
    {
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to alloc encoder"), this);
        return;
    }

    // Configure encoder: 44.1kHz stereo, 192kbps MP3
    enc_ctx->sample_rate = 44100;
    enc_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
    enc_ctx->channels = 2;
    // Use FLTP (float planar) which is well-supported by libmp3lame
    enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
    enc_ctx->bit_rate = 192000;

    if (int ret = avcodec_open2(enc_ctx, enc, nullptr))
    {
        std::cerr << "  ERROR: avcodec_open2 (encoder): " << av_err_to_string(ret) << std::endl;
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to open encoder"), this);
        return;
    }

    // Build libavfilter graph
    AVFilterGraph *graph = avfilter_graph_alloc();
    if (!graph)
    {
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to alloc filter graph"), this);
        return;
    }

    // Source filter (input) - abuffer
    uint64_t channel_layout = dec_ctx->channel_layout;
    if (!channel_layout)
    {
        channel_layout = av_get_default_channel_layout(dec_ctx->channels);
    }

    char src_args[512];
    snprintf(src_args, sizeof(src_args),
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
             1, dec_ctx->sample_rate,
             dec_ctx->sample_rate,
             av_get_sample_fmt_name(dec_ctx->sample_fmt),
             channel_layout);

    std::cout << "  Filter source args: " << src_args << std::endl;

    AVFilterContext *src_ctx = nullptr;
    const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    if (!abuffer)
    {
        std::cerr << "  ERROR: abuffer filter not found" << std::endl;
        avfilter_graph_free(&graph);
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "abuffer filter not found"), this);
        return;
    }

    if (int ret = avfilter_graph_create_filter(&src_ctx, abuffer, "in", src_args, nullptr, graph))
    {
        std::cerr << "  ERROR: failed to create abuffer: " << av_err_to_string(ret) << std::endl;
        avfilter_graph_free(&graph);
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to create source filter"), this);
        return;
    }

    // Sink filter (output) - abuffersink
    AVFilterContext *sink_ctx = nullptr;
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    if (!abuffersink)
    {
        std::cerr << "  ERROR: abuffersink filter not found" << std::endl;
        avfilter_graph_free(&graph);
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "abuffersink filter not found"), this);
        return;
    }

    if (int ret = avfilter_graph_create_filter(&sink_ctx, abuffersink, "out", nullptr, nullptr, graph))
    {
        std::cerr << "  ERROR: failed to create abuffersink: " << av_err_to_string(ret) << std::endl;
        avfilter_graph_free(&graph);
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to create sink filter"), this);
        return;
    }

    // Set output format constraints on the sink to match encoder requirements
    static const enum AVSampleFormat out_sample_fmts[] = {AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE};
    static const int64_t out_channel_layouts[] = {AV_CH_LAYOUT_STEREO, -1};
    static const int out_sample_rates[] = {44100, -1};

    av_opt_set_int_list(sink_ctx, "sample_fmts", out_sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    av_opt_set_int_list(sink_ctx, "channel_layouts", out_channel_layouts, -1, AV_OPT_SEARCH_CHILDREN);
    av_opt_set_int_list(sink_ctx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN);

    // Build filter chain by creating and linking filters manually
    AVFilterContext *last_ctx = src_ctx;

    // Create atempo filter if speed != 1.0
    if (speed != 1.0f)
    {
        char atempo_args[64];
        snprintf(atempo_args, sizeof(atempo_args), "tempo=%.2f", speed);

        const AVFilter *atempo_filter = avfilter_get_by_name("atempo");
        if (!atempo_filter)
        {
            std::cerr << "  ERROR: atempo filter not found" << std::endl;
            avfilter_graph_free(&graph);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            status_ = FINISH;
            writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "atempo filter not found"), this);
            return;
        }

        AVFilterContext *atempo_ctx = nullptr;
        if (int ret = avfilter_graph_create_filter(&atempo_ctx, atempo_filter, "atempo", atempo_args, nullptr, graph))
        {
            std::cerr << "  ERROR: failed to create atempo filter: " << av_err_to_string(ret) << std::endl;
            avfilter_graph_free(&graph);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            status_ = FINISH;
            writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to create atempo filter"), this);
            return;
        }

        if (int ret = avfilter_link(last_ctx, 0, atempo_ctx, 0))
        {
            std::cerr << "  ERROR: failed to link to atempo: " << av_err_to_string(ret) << std::endl;
            avfilter_graph_free(&graph);
            avcodec_free_context(&enc_ctx);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            status_ = FINISH;
            writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to link atempo filter"), this);
            return;
        }
        last_ctx = atempo_ctx;
        std::cout << "  Added atempo filter: " << atempo_args << std::endl;
    }

    // Create rubberband filter if pitch != 1.0 (if available)
    if (pitch != 1.0f)
    {
        const AVFilter *rubberband_filter = avfilter_get_by_name("rubberband");
        if (rubberband_filter)
        {
            char rubberband_args[128];
            snprintf(rubberband_args, sizeof(rubberband_args), "pitch=%.2f", pitch);

            AVFilterContext *rubberband_ctx = nullptr;
            if (int ret = avfilter_graph_create_filter(&rubberband_ctx, rubberband_filter, "rubberband", rubberband_args, nullptr, graph))
            {
                std::cerr << "  WARNING: failed to create rubberband filter: " << av_err_to_string(ret) << std::endl;
                std::cerr << "  Pitch shifting will be skipped" << std::endl;
            }
            else
            {
                if (int ret = avfilter_link(last_ctx, 0, rubberband_ctx, 0))
                {
                    std::cerr << "  WARNING: failed to link to rubberband: " << av_err_to_string(ret) << std::endl;
                }
                else
                {
                    last_ctx = rubberband_ctx;
                    std::cout << "  Added rubberband filter: " << rubberband_args << std::endl;
                }
            }
        }
        else
        {
            std::cerr << "  WARNING: rubberband filter not available, pitch shifting skipped" << std::endl;
        }
    }

    // Create aformat filter to convert to encoder's expected format
    char aformat_args[256];
    snprintf(aformat_args, sizeof(aformat_args),
             "sample_fmts=%s:sample_rates=%d:channel_layouts=stereo",
             av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP),
             enc_ctx->sample_rate);

    const AVFilter *aformat_filter = avfilter_get_by_name("aformat");
    if (aformat_filter)
    {
        AVFilterContext *aformat_ctx = nullptr;
        if (int ret = avfilter_graph_create_filter(&aformat_ctx, aformat_filter, "aformat", aformat_args, nullptr, graph))
        {
            std::cerr << "  WARNING: failed to create aformat filter: " << av_err_to_string(ret) << std::endl;
        }
        else
        {
            if (int ret = avfilter_link(last_ctx, 0, aformat_ctx, 0))
            {
                std::cerr << "  WARNING: failed to link to aformat: " << av_err_to_string(ret) << std::endl;
            }
            else
            {
                last_ctx = aformat_ctx;
                std::cout << "  Added aformat filter: " << aformat_args << std::endl;
            }
        }
    }

    // Add asetnsamples filter to ensure exactly 1152 samples per frame (required by MP3 encoder)
    const AVFilter *asetnsamples_filter = avfilter_get_by_name("asetnsamples");
    if (asetnsamples_filter)
    {
        AVFilterContext *asetnsamples_ctx = nullptr;
        // MP3 frame size is 1152 samples, pad the last frame
        if (int ret = avfilter_graph_create_filter(&asetnsamples_ctx, asetnsamples_filter, "asetnsamples", "n=1152:p=1", nullptr, graph))
        {
            std::cerr << "  WARNING: failed to create asetnsamples filter: " << av_err_to_string(ret) << std::endl;
        }
        else
        {
            if (int ret = avfilter_link(last_ctx, 0, asetnsamples_ctx, 0))
            {
                std::cerr << "  WARNING: failed to link to asetnsamples: " << av_err_to_string(ret) << std::endl;
            }
            else
            {
                last_ctx = asetnsamples_ctx;
                std::cout << "  Added asetnsamples filter: n=1152:p=1" << std::endl;
            }
        }
    }

    // Link the last filter to the sink
    if (int ret = avfilter_link(last_ctx, 0, sink_ctx, 0))
    {
        std::cerr << "  ERROR: failed to link to sink: " << av_err_to_string(ret) << std::endl;
        avfilter_graph_free(&graph);
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to link to sink"), this);
        return;
    }

    // Configure the filter graph
    if (int ret = avfilter_graph_config(graph, nullptr))
    {
        std::cerr << "  ERROR: avfilter_graph_config: " << av_err_to_string(ret) << std::endl;
        avfilter_graph_free(&graph);
        avcodec_free_context(&enc_ctx);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&in_fmt);
        status_ = FINISH;
        writer_.Finish(grpc::Status(grpc::StatusCode::INTERNAL, "Failed to configure filter graph"), this);
        return;
    }

    std::cout << "  Filter graph configured successfully" << std::endl;

    // Store for processing
    streaming_in_fmt_ = in_fmt;
    streaming_audio_stream_idx_ = audio_stream_index;
    streaming_dec_ctx_ = dec_ctx;
    streaming_enc_ctx_ = enc_ctx;
    streaming_graph_ = graph;
    streaming_src_ctx_ = src_ctx;
    streaming_sink_ctx_ = sink_ctx;
    streaming_filter_desc_ = filter_desc;
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::send_next_chunk()
{
    // Handle no-effects passthrough
    if (streaming_no_effects_)
    {
        // Read directly from input file and stream as MP3 chunks
        if (!ffmpeg_pipe_)
        {
            std::string cmd = "ffmpeg -i \"" + request_.audio_path() + "\" -acodec libmp3lame -ab 192k -ar 44100 -f mp3 - 2>/dev/null";
            ffmpeg_pipe_ = popen(cmd.c_str(), "r");
            if (!ffmpeg_pipe_)
            {
                finish_stream();
                return;
            }
        }

        size_t bytes_read = fread(ffmpeg_buffer_, 1, sizeof(ffmpeg_buffer_), ffmpeg_pipe_);
        if (bytes_read > 0)
        {
            soundboard::AudioChunk chunk;
            chunk.set_data(ffmpeg_buffer_, bytes_read);
            chunk.set_sequence_number(chunk_sequence_++);
            status_ = WRITING;
            writer_.Write(chunk, this);
        }
        else
        {
            finish_stream();
        }
        return;
    }

    // libavfilter streaming path
    if (!streaming_frame_)
    {
        streaming_frame_ = av_frame_alloc();
        streaming_filtered_frame_ = av_frame_alloc();
    }

    bool got_output = false;

    // Helper lambda to try encoding filtered frames and sending output
    auto try_encode_and_send = [&]() -> bool
    {
        while (av_buffersink_get_frame(streaming_sink_ctx_, streaming_filtered_frame_) >= 0)
        {
            // Set proper PTS for the encoder
            streaming_filtered_frame_->pts = streaming_pts_;
            streaming_pts_ += streaming_filtered_frame_->nb_samples;

            int ret = avcodec_send_frame(streaming_enc_ctx_, streaming_filtered_frame_);
            av_frame_unref(streaming_filtered_frame_);

            if (ret < 0)
            {
                continue;
            }

            AVPacket *enc_pkt = av_packet_alloc();
            while (avcodec_receive_packet(streaming_enc_ctx_, enc_pkt) >= 0)
            {
                soundboard::AudioChunk chunk;
                chunk.set_data(enc_pkt->data, enc_pkt->size);
                chunk.set_sequence_number(chunk_sequence_++);
                status_ = WRITING;
                av_packet_unref(enc_pkt);
                av_packet_free(&enc_pkt);
                writer_.Write(chunk, this);
                return true;
            }
            av_packet_free(&enc_pkt);
        }
        return false;
    };

    // Phase 1: Read and decode packets from input
    if (!decoder_flushed_)
    {
        AVPacket *pkt = av_packet_alloc();
        int read_ret = av_read_frame(streaming_in_fmt_, pkt);

        if (read_ret >= 0)
        {
            if (pkt->stream_index == streaming_audio_stream_idx_)
            {
                avcodec_send_packet(streaming_dec_ctx_, pkt);

                while (avcodec_receive_frame(streaming_dec_ctx_, streaming_frame_) == 0)
                {
                    // Push frame to filter graph
                    if (av_buffersrc_add_frame_flags(streaming_src_ctx_, streaming_frame_, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                    {
                        av_frame_unref(streaming_frame_);
                        break;
                    }
                    av_frame_unref(streaming_frame_);

                    // Try to get filtered output and encode
                    if (try_encode_and_send())
                    {
                        got_output = true;
                        break;
                    }
                }
            }
            av_packet_unref(pkt);
        }
        else
        {
            // End of input - start flushing decoder
            decoder_flushed_ = true;
            avcodec_send_packet(streaming_dec_ctx_, nullptr);
        }
        av_packet_free(&pkt);
    }

    // Phase 2: Flush decoder
    if (!got_output && decoder_flushed_ && !filter_flushed_)
    {
        while (avcodec_receive_frame(streaming_dec_ctx_, streaming_frame_) == 0)
        {
            if (av_buffersrc_add_frame_flags(streaming_src_ctx_, streaming_frame_, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
            {
                av_frame_unref(streaming_frame_);
                break;
            }
            av_frame_unref(streaming_frame_);

            if (try_encode_and_send())
            {
                got_output = true;
                break;
            }
        }

        if (!got_output)
        {
            // Flush the filter graph
            filter_flushed_ = true;
            if (av_buffersrc_add_frame_flags(streaming_src_ctx_, nullptr, 0) < 0)
            {
                // Filter flushing failed, continue anyway
            }
        }
    }

    // Phase 3: Flush filter graph
    if (!got_output && filter_flushed_ && !encoder_flushed_)
    {
        if (try_encode_and_send())
        {
            got_output = true;
        }
        else
        {
            // Flush encoder
            encoder_flushed_ = true;
            avcodec_send_frame(streaming_enc_ctx_, nullptr);
        }
    }

    // Phase 4: Flush encoder
    if (!got_output && encoder_flushed_)
    {
        AVPacket *enc_pkt = av_packet_alloc();
        if (avcodec_receive_packet(streaming_enc_ctx_, enc_pkt) >= 0)
        {
            soundboard::AudioChunk chunk;
            chunk.set_data(enc_pkt->data, enc_pkt->size);
            chunk.set_sequence_number(chunk_sequence_++);
            status_ = WRITING;
            av_packet_unref(enc_pkt);
            av_packet_free(&enc_pkt);
            writer_.Write(chunk, this);
            got_output = true;
        }
        else
        {
            av_packet_free(&enc_pkt);
            finish_stream();
            return;
        }
    }

    // If no output was produced in phase 1, continue processing
    if (!got_output && !decoder_flushed_)
    {
        // Continue to next iteration
        send_next_chunk();
    }
}

void AudioProcessorAsync::ApplyEffectsStreamCallData::finish_stream()
{
    status_ = FINISH;
    std::cout << "  Result: SUCCESS (streamed " << chunk_sequence_ << " chunks)" << std::endl;

    // Cleanup libavfilter resources
    if (streaming_frame_)
        av_frame_free(&streaming_frame_);
    if (streaming_filtered_frame_)
        av_frame_free(&streaming_filtered_frame_);
    if (streaming_graph_)
        avfilter_graph_free(&streaming_graph_);
    if (streaming_enc_ctx_)
        avcodec_free_context(&streaming_enc_ctx_);
    if (streaming_dec_ctx_)
        avcodec_free_context(&streaming_dec_ctx_);
    if (streaming_in_fmt_)
        avformat_close_input(&streaming_in_fmt_);
    if (ffmpeg_pipe_)
    {
        pclose(ffmpeg_pipe_);
        ffmpeg_pipe_ = nullptr;
    }

    writer_.Finish(grpc::Status::OK, this);
}
