#include "audio_conversion.h"
#include <iostream>
#include <cstring>

// FFmpeg is a C library 
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

namespace soundboard
{

    // Helper to format FFmpeg error codes
    static std::string av_err_to_string(int errnum)
    {
        char buf[256];
        av_strerror(errnum, buf, sizeof(buf));
        return std::string(buf);
    }

    // Convert input media to MP3 using libav
    bool convert_to_mp3_libav(const std::string &in_path,
                              const std::string &out_path,
                              int bitrate_kbps,
                              std::string &error_out)
    {
        AVFormatContext *in_fmt = nullptr;
        if (int ret = avformat_open_input(&in_fmt, in_path.c_str(), nullptr, nullptr))
        {
            error_out = "avformat_open_input: " + av_err_to_string(ret);
            return false;
        }

        if (int ret = avformat_find_stream_info(in_fmt, nullptr))
        {
            error_out = "avformat_find_stream_info: " + av_err_to_string(ret);
            avformat_close_input(&in_fmt);
            return false;
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
            error_out = "no audio stream found";
            avformat_close_input(&in_fmt);
            return false;
        }

        AVStream *in_stream = in_fmt->streams[audio_stream_index];
        const AVCodec *dec = avcodec_find_decoder(in_stream->codecpar->codec_id);
        if (!dec)
        {
            error_out = "decoder not found";
            avformat_close_input(&in_fmt);
            return false;
        }

        AVCodecContext *dec_ctx = avcodec_alloc_context3(dec);
        if (!dec_ctx)
        {
            error_out = "failed to alloc decoder context";
            avformat_close_input(&in_fmt);
            return false;
        }
        if (int ret = avcodec_parameters_to_context(dec_ctx, in_stream->codecpar))
        {
            error_out = "avcodec_parameters_to_context: " + av_err_to_string(ret);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }
        if (int ret = avcodec_open2(dec_ctx, dec, nullptr))
        {
            error_out = "avcodec_open2 (decoder): " + av_err_to_string(ret);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        // Setup output context
        AVFormatContext *out_fmt = nullptr;
        if (int ret = avformat_alloc_output_context2(&out_fmt, nullptr, nullptr, out_path.c_str()))
        {
            error_out = "avformat_alloc_output_context2: " + av_err_to_string(ret);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        const AVCodec *enc = avcodec_find_encoder(AV_CODEC_ID_MP3);
        if (!enc)
        {
            error_out = "MP3 encoder not found";
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        AVStream *out_stream = avformat_new_stream(out_fmt, nullptr);
        if (!out_stream)
        {
            error_out = "failed to create output stream";
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        AVCodecContext *enc_ctx = avcodec_alloc_context3(enc);
        if (!enc_ctx)
        {
            error_out = "failed to alloc encoder context";
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        // Configure encoder parameters, use FLTP to match what we need
        enc_ctx->sample_rate = 44100;
        enc_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
        enc_ctx->channels = 2;
        enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        enc_ctx->bit_rate = bitrate_kbps * 1000;

        // Some containers require global header
        if (out_fmt->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (int ret = avcodec_open2(enc_ctx, enc, nullptr))
        {
            error_out = "avcodec_open2 (encoder): " + av_err_to_string(ret);
            avcodec_free_context(&enc_ctx);
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        // Copy encoder params to stream
        if (int ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx))
        {
            error_out = "avcodec_parameters_from_context: " + av_err_to_string(ret);
            avcodec_free_context(&enc_ctx);
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }
        out_stream->time_base = AVRational{1, enc_ctx->sample_rate};

        // Open output file
        if (!(out_fmt->oformat->flags & AVFMT_NOFILE))
        {
            if (int ret = avio_open(&out_fmt->pb, out_path.c_str(), AVIO_FLAG_WRITE))
            {
                error_out = "avio_open: " + av_err_to_string(ret);
                avcodec_free_context(&enc_ctx);
                avformat_free_context(out_fmt);
                avcodec_free_context(&dec_ctx);
                avformat_close_input(&in_fmt);
                return false;
            }
        }

        if (int ret = avformat_write_header(out_fmt, nullptr))
        {
            error_out = "avformat_write_header: " + av_err_to_string(ret);
            if (out_fmt->pb)
                avio_closep(&out_fmt->pb);
            avcodec_free_context(&enc_ctx);
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        // Setup resampler - handle decoder output to encoder input
        uint64_t dec_channel_layout = dec_ctx->channel_layout;
        if (!dec_channel_layout)
            dec_channel_layout = av_get_default_channel_layout(dec_ctx->channels);

        SwrContext *swr = swr_alloc_set_opts(nullptr,
                                             enc_ctx->channel_layout, enc_ctx->sample_fmt, enc_ctx->sample_rate,
                                             dec_channel_layout, dec_ctx->sample_fmt, dec_ctx->sample_rate,
                                             0, nullptr);
        if (!swr)
        {
            error_out = "swr_alloc_set_opts failed";
            if (out_fmt->pb)
                avio_closep(&out_fmt->pb);
            avcodec_free_context(&enc_ctx);
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }
        if (int ret = swr_init(swr))
        {
            error_out = "swr_init: " + av_err_to_string(ret);
            swr_free(&swr);
            if (out_fmt->pb)
                avio_closep(&out_fmt->pb);
            avcodec_free_context(&enc_ctx);
            avformat_free_context(out_fmt);
            avcodec_free_context(&dec_ctx);
            avformat_close_input(&in_fmt);
            return false;
        }

        AVPacket *pkt = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        AVFrame *resampled = av_frame_alloc();
        int64_t pts = 0;

        // Helper lambda to encode and write a frame
        auto encode_and_write = [&](AVFrame *f) -> bool
        {
            if (avcodec_send_frame(enc_ctx, f) < 0)
                return false;

            AVPacket *out_pkt = av_packet_alloc();
            while (avcodec_receive_packet(enc_ctx, out_pkt) == 0)
            {
                out_pkt->stream_index = out_stream->index;
                av_packet_rescale_ts(out_pkt, enc_ctx->time_base, out_stream->time_base);
                if (av_interleaved_write_frame(out_fmt, out_pkt) < 0)
                {
                    av_packet_free(&out_pkt);
                    return false;
                }
            }
            av_packet_free(&out_pkt);
            return true;
        };

        // Process all packets from input
        while (av_read_frame(in_fmt, pkt) >= 0)
        {
            if (pkt->stream_index == audio_stream_index)
            {
                avcodec_send_packet(dec_ctx, pkt);

                while (avcodec_receive_frame(dec_ctx, frame) == 0)
                {
                    // Resample frame
                    int dst_nb_samples = av_rescale_rnd(
                        swr_get_delay(swr, dec_ctx->sample_rate) + frame->nb_samples,
                        enc_ctx->sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);

                    resampled->channel_layout = enc_ctx->channel_layout;
                    resampled->sample_rate = enc_ctx->sample_rate;
                    resampled->format = enc_ctx->sample_fmt;
                    resampled->nb_samples = dst_nb_samples;

                    if (av_frame_get_buffer(resampled, 0) < 0)
                    {
                        error_out = "av_frame_get_buffer failed";
                        goto cleanup;
                    }

                    if (swr_convert_frame(swr, resampled, frame) < 0)
                    {
                        error_out = "swr_convert_frame failed";
                        goto cleanup;
                    }

                    resampled->pts = pts;
                    pts += resampled->nb_samples;

                    if (!encode_and_write(resampled))
                    {
                        error_out = "encode_and_write failed";
                        av_frame_unref(resampled);
                        goto cleanup;
                    }

                    av_frame_unref(resampled);
                    av_frame_unref(frame);
                }
            }
            av_packet_unref(pkt);
        }

        // Flush decoder
        avcodec_send_packet(dec_ctx, nullptr);
        while (avcodec_receive_frame(dec_ctx, frame) == 0)
        {
            // Resample leftover frame
            int dst_nb_samples = av_rescale_rnd(
                swr_get_delay(swr, dec_ctx->sample_rate) + frame->nb_samples,
                enc_ctx->sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);

            resampled->channel_layout = enc_ctx->channel_layout;
            resampled->sample_rate = enc_ctx->sample_rate;
            resampled->format = enc_ctx->sample_fmt;
            resampled->nb_samples = dst_nb_samples;

            if (av_frame_get_buffer(resampled, 0) < 0)
            {
                error_out = "av_frame_get_buffer failed (flush)";
                av_frame_unref(frame);
                goto cleanup;
            }

            if (swr_convert_frame(swr, resampled, frame) < 0)
            {
                error_out = "swr_convert_frame failed (flush)";
                av_frame_unref(frame);
                goto cleanup;
            }

            resampled->pts = pts;
            pts += resampled->nb_samples;

            if (!encode_and_write(resampled))
            {
                error_out = "encode_and_write failed (flush)";
                av_frame_unref(resampled);
                av_frame_unref(frame);
                goto cleanup;
            }

            av_frame_unref(resampled);
            av_frame_unref(frame);
        }

        // Flush resampler (in case there are leftover samples)
        {
            int dst_nb_samples = av_rescale_rnd(
                swr_get_delay(swr, dec_ctx->sample_rate),
                enc_ctx->sample_rate, dec_ctx->sample_rate, AV_ROUND_UP);

            if (dst_nb_samples > 0)
            {
                resampled->channel_layout = enc_ctx->channel_layout;
                resampled->sample_rate = enc_ctx->sample_rate;
                resampled->format = enc_ctx->sample_fmt;
                resampled->nb_samples = dst_nb_samples;

                if (av_frame_get_buffer(resampled, 0) == 0 &&
                    swr_convert_frame(swr, resampled, nullptr) == 0)
                {
                    resampled->pts = pts;
                    pts += resampled->nb_samples;
                    encode_and_write(resampled);
                    av_frame_unref(resampled);
                }
            }
        }

        // Flush encoder
        {
            avcodec_send_frame(enc_ctx, nullptr);
            AVPacket *out_pkt = av_packet_alloc();
            while (avcodec_receive_packet(enc_ctx, out_pkt) == 0)
            {
                out_pkt->stream_index = out_stream->index;
                av_packet_rescale_ts(out_pkt, enc_ctx->time_base, out_stream->time_base);
                if (av_interleaved_write_frame(out_fmt, out_pkt) < 0)
                {
                    error_out = "av_interleaved_write_frame failed (encoder flush)";
                    av_packet_free(&out_pkt);
                    goto cleanup;
                }
            }
            av_packet_free(&out_pkt);
        }

        av_write_trailer(out_fmt);

    cleanup:
        if (pkt)
            av_packet_free(&pkt);
        if (frame)
            av_frame_free(&frame);
        if (resampled)
            av_frame_free(&resampled);
        if (swr)
            swr_free(&swr);
        if (out_fmt)
        {
            if (out_fmt->pb)
                avio_closep(&out_fmt->pb);
            avformat_free_context(out_fmt);
        }
        if (enc_ctx)
            avcodec_free_context(&enc_ctx);
        if (dec_ctx)
            avcodec_free_context(&dec_ctx);
        if (in_fmt)
            avformat_close_input(&in_fmt);

        return error_out.empty();
    }

}
