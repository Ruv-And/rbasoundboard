package com.soundboard.service;

import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import soundboard.AudioProcessorGrpc;
import soundboard.AudioProcessorOuterClass;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Iterator;
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
    
    /**
     * Apply speed and pitch effects to audio file with streaming (no disk storage)
     * Streams audio chunks from C++ processor directly to the provided OutputStream
     * 
     * @param audioPath Path to the input audio file
     * @param speedFactor Speed multiplier (0.5 to 2.0, 1.0 = normal)
     * @param pitchFactor Pitch multiplier (0.5 to 2.0, 1.0 = normal)
     * @param outputStream OutputStream to write processed audio chunks to
     * @throws IOException if writing to outputStream fails
     * @throws Exception if processing fails
     */
        public void applyEffectsStream(String audioPath, float speedFactor, float pitchFactor, OutputStream outputStream) 
            throws IOException, Exception {
        log.info("Applying effects with streaming: speed={}, pitch={} for {}", speedFactor, pitchFactor, audioPath);
        
        AudioProcessorOuterClass.ApplyEffectsRequest request = AudioProcessorOuterClass.ApplyEffectsRequest.newBuilder()
                .setAudioPath(audioPath)
                .setSpeedFactor(speedFactor)
                .setPitchFactor(pitchFactor)
                .build();
        
        try {
            Iterator<AudioProcessorOuterClass.AudioChunk> chunks = blockingStub.applyEffectsStream(request);
            
            int chunkCount = 0;
            while (chunks.hasNext()) {
                AudioProcessorOuterClass.AudioChunk chunk = chunks.next();
                outputStream.write(chunk.getData().toByteArray());
                chunkCount++;
            }
            
            log.info("Effects applied and streamed successfully: {} chunks", chunkCount);
            outputStream.flush();
            
        } catch (StatusRuntimeException sre) {
            if (sre.getStatus().getCode() == Status.Code.RESOURCE_EXHAUSTED) {
                log.warn("Audio processor is busy (RESOURCE_EXHAUSTED)");
                throw new ProcessorBusyException("Audio processor is busy, please try again later");
            }
            log.error("gRPC status error applying effects", sre);
            throw new Exception("Failed to stream processed audio: " + sre.getStatus(), sre);
        } catch (Exception e) {
            log.error("Unexpected error applying effects", e);
            throw new Exception("Failed to stream processed audio: " + e.getMessage(), e);
        }
    }

    /**
     * Begin streaming with an early probe to map RESOURCE_EXHAUSTED before sending headers.
     * Returns a session containing the iterator and the first chunk (if any) already fetched.
     */
    public AudioStreamSession beginApplyEffectsStream(String audioPath, float speedFactor, float pitchFactor)
            throws ProcessorBusyException, Exception {
        log.info("Begin applyEffectsStream (probe) speed={}, pitch={} for {}", speedFactor, pitchFactor, audioPath);

        AudioProcessorOuterClass.ApplyEffectsRequest request = AudioProcessorOuterClass.ApplyEffectsRequest.newBuilder()
                .setAudioPath(audioPath)
                .setSpeedFactor(speedFactor)
                .setPitchFactor(pitchFactor)
                .build();
        try {
            Iterator<AudioProcessorOuterClass.AudioChunk> it = blockingStub.applyEffectsStream(request);
            byte[] first = null;
            // Trigger the stream to surface immediate errors (e.g., RESOURCE_EXHAUSTED)
            if (it.hasNext()) {
                AudioProcessorOuterClass.AudioChunk firstChunk = it.next();
                first = firstChunk.getData().toByteArray();
            }
            return new AudioStreamSession(it, first);
        } catch (StatusRuntimeException sre) {
            if (sre.getStatus().getCode() == Status.Code.RESOURCE_EXHAUSTED) {
                log.warn("Audio processor is busy (RESOURCE_EXHAUSTED) on probe");
                throw new ProcessorBusyException("Audio processor is busy, please try again later");
            }
            log.error("gRPC status error on probe", sre);
            throw new Exception("Failed to initiate processed audio stream: " + sre.getStatus(), sre);
        } catch (Exception e) {
            log.error("Unexpected error on probe", e);
            throw new Exception("Failed to initiate processed audio stream: " + e.getMessage(), e);
        }
    }
}

