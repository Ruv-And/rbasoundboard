#pragma once

#include <string>

namespace soundboard
{

    /**
     * Convert input audio file to MP3 format using FFmpeg's libav* libraries.
     * Supports any audio format with available FFmpeg decoder (WAV, AAC, FLAC, OGG, etc.)
     *
     * @param in_path Path to input audio file
     * @param out_path Path to output MP3 file
     * @param bitrate_kbps Target bitrate (192kbps)
     * @param error_out Reference to string with error message
     * @return true on success, false on fail
     */
    bool convert_to_mp3_libav(const std::string &in_path,
                              const std::string &out_path,
                              int bitrate_kbps,
                              std::string &error_out);

}
