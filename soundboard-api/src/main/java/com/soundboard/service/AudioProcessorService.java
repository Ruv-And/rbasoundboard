package com.soundboard.service;

import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import soundboard.AudioProcessorGrpc;
import soundboard.AudioProcessorOuterClass;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

@Service
@Slf4j
public class AudioProcessorService {

    @Value("${grpc.audio-processor.host}")
    private String audioProcessorHost;

    @Value("${grpc.audio-processor.port}")
    private int audioProcessorPort;

    @Value("${storage.audio-dir}")
    private String audioDir;

    private ManagedChannel channel;
    private AudioProcessorGrpc.AudioProcessorBlockingStub blockingStub;

    @PostConstruct
    public void init() {
        log.info("Initializing gRPC channel to audio processor at {}:{}", audioProcessorHost, audioProcessorPort);
        channel = ManagedChannelBuilder
                .forAddress(audioProcessorHost, audioProcessorPort)
                .usePlaintext()
                .build();
        blockingStub = AudioProcessorGrpc.newBlockingStub(channel);
    }

    @PreDestroy
    public void shutdown() throws InterruptedException {
        if (channel != null) {
            log.info("Shutting down gRPC channel");
            channel.shutdown().awaitTermination(5, TimeUnit.SECONDS);
        }
    }

    /**
     * Extract audio from a video file using FFmpeg via gRPC
     * 
     * @param videoPath Path to the input video file
     * @param format    Output audio format (mp3, wav, ogg)
     * @param bitrate   Audio bitrate in kbps (e.g., 192)
     * @return Path to the extracted audio file
     * @throws Exception if extraction fails
     */
    public String extractAudio(String videoPath, String format, int bitrate) throws Exception {
        // Generate unique output filename
        String outputFilename = UUID.randomUUID().toString() + "." + format;
        String outputPath = audioDir + "/" + outputFilename;

        log.info("Requesting audio extraction: {} -> {} ({}kbps)", videoPath, outputPath, bitrate);

        AudioProcessorOuterClass.ExtractAudioRequest request = AudioProcessorOuterClass.ExtractAudioRequest.newBuilder()
                .setVideoPath(videoPath)
                .setOutputPath(outputPath)
                .setFormat(format)
                .setBitrateKbps(bitrate)
                .build();

        AudioProcessorOuterClass.ExtractAudioResponse response;
        try {
            response = blockingStub.extractAudio(request);
        } catch (Exception e) {
            log.error("gRPC call to extractAudio failed", e);
            throw new Exception("Failed to communicate with audio processor: " + e.getMessage(), e);
        }

        if (!response.getSuccess()) {
            String errorMsg = response.getErrorMessage();
            log.error("Audio extraction failed: {}", errorMsg);
            throw new Exception("Audio extraction failed: " + errorMsg);
        }

        log.info("Audio extracted successfully: {} ({} bytes, {} seconds)",
                response.getAudioPath(),
                response.getFileSizeBytes(),
                response.getDurationSeconds());

        return response.getAudioPath();
    }

    /**
     * Get audio file information
     * 
     * @param audioPath Path to the audio file
     * @return Audio metadata
     */
    public AudioProcessorOuterClass.AudioInfoResponse getAudioInfo(String audioPath) {
        log.info("Getting audio info for: {}", audioPath);

        AudioProcessorOuterClass.AudioInfoRequest request = AudioProcessorOuterClass.AudioInfoRequest.newBuilder()
                .setAudioPath(audioPath)
                .build();

        return blockingStub.getAudioInfo(request);
    }
}
