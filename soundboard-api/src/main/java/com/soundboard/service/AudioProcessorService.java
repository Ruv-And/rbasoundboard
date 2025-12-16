package com.soundboard.service;

import io.grpc.ManagedChannel;
import io.grpc.ManagedChannelBuilder;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.stub.StreamObserver;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import soundboard.AudioProcessorGrpc;
import soundboard.AudioProcessorOuterClass;

import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import java.io.IOException;
import java.io.OutputStream;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

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
    private AudioProcessorGrpc.AudioProcessorStub asyncStub;

    @PostConstruct
    public void init() {
        log.info("Initializing gRPC channel to audio processor at {}:{}", audioProcessorHost, audioProcessorPort);
        channel = ManagedChannelBuilder
                .forAddress(audioProcessorHost, audioProcessorPort)
                .usePlaintext()
                .build();
        asyncStub = AudioProcessorGrpc.newStub(channel);
    }

    @PreDestroy
    public void shutdown() throws InterruptedException {
        if (channel != null) {
            log.info("Shutting down gRPC channel");
            channel.shutdown().awaitTermination(5, TimeUnit.SECONDS);
        }
    }

    /**
     * Extract audio from a video file using FFmpeg via gRPC (async)
     * 
     * @param videoPath Path to the input video file
     * @param format    Output audio format (mp3, wav, ogg)
     * @param bitrate   Audio bitrate in kbps (e.g., 192)
     * @return Path to the extracted audio file
     * @throws Exception if extraction fails
     */
    public String extractAudio(String videoPath, String format, int bitrate) throws Exception {
        log.info("Requesting audio extraction: {} -> {} ({}kbps)", videoPath, format, bitrate);

        String safeFormat = (format == null || format.isBlank()) ? "mp3" : format.toLowerCase();
        String outputFilename = UUID.randomUUID() + "." + safeFormat;
        String outputPath = audioDir + "/" + outputFilename;

        AudioProcessorOuterClass.ExtractAudioRequest request = AudioProcessorOuterClass.ExtractAudioRequest.newBuilder()
            .setVideoPath(videoPath)
            .setOutputPath(outputPath)
            .setFormat(safeFormat)
            .setBitrateKbps(bitrate)
            .build();

        CountDownLatch latch = new CountDownLatch(1);
        AtomicReference<AudioProcessorOuterClass.ExtractAudioResponse> responseRef = new AtomicReference<>();
        AtomicReference<Throwable> errorRef = new AtomicReference<>();

        asyncStub.extractAudio(request, new StreamObserver<AudioProcessorOuterClass.ExtractAudioResponse>() {
            @Override
            public void onNext(AudioProcessorOuterClass.ExtractAudioResponse response) {
                responseRef.set(response);
            }

            @Override
            public void onError(Throwable t) {
                errorRef.set(t);
                latch.countDown();
            }

            @Override
            public void onCompleted() {
                latch.countDown();
            }
        });

        if (!latch.await(60, TimeUnit.SECONDS)) {
            throw new Exception("Audio extraction timed out");
        }

        if (errorRef.get() != null) {
            Throwable err = errorRef.get();
            if (err instanceof StatusRuntimeException) {
                StatusRuntimeException sre = (StatusRuntimeException) err;
                if (sre.getStatus().getCode() == Status.Code.RESOURCE_EXHAUSTED) {
                    throw new ProcessorBusyException("Audio processor is busy, please try again later");
                }
            }
            log.error("gRPC call to extractAudio failed", err);
            throw new Exception("Failed to communicate with audio processor: " + err.getMessage(), err);
        }

        AudioProcessorOuterClass.ExtractAudioResponse response = responseRef.get();
        if (response == null || !response.getSuccess()) {
            String errorMsg = response != null ? response.getErrorMessage() : "Unknown error";
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
     * Get audio file information via async gRPC
     */
    public AudioProcessorOuterClass.AudioInfoResponse getAudioInfo(String audioPath) throws Exception {
        log.info("Getting audio info for: {}", audioPath);

        AudioProcessorOuterClass.AudioInfoRequest request = AudioProcessorOuterClass.AudioInfoRequest.newBuilder()
                .setAudioPath(audioPath)
                .build();

        CountDownLatch latch = new CountDownLatch(1);
        AtomicReference<AudioProcessorOuterClass.AudioInfoResponse> responseRef = new AtomicReference<>();
        AtomicReference<Throwable> errorRef = new AtomicReference<>();

        asyncStub.getAudioInfo(request, new StreamObserver<AudioProcessorOuterClass.AudioInfoResponse>() {
            @Override
            public void onNext(AudioProcessorOuterClass.AudioInfoResponse value) {
                responseRef.set(value);
            }

            @Override
            public void onError(Throwable t) {
                errorRef.set(t);
                latch.countDown();
            }

            @Override
            public void onCompleted() {
                latch.countDown();
            }
        });

        if (!latch.await(30, TimeUnit.SECONDS)) {
            throw new Exception("getAudioInfo timed out");
        }

        if (errorRef.get() != null) {
            Throwable err = errorRef.get();
            if (err instanceof StatusRuntimeException) {
                StatusRuntimeException sre = (StatusRuntimeException) err;
                if (sre.getStatus().getCode() == Status.Code.RESOURCE_EXHAUSTED) {
                    throw new ProcessorBusyException("Audio processor is busy, please try again later");
                }
            }
            log.error("gRPC call to getAudioInfo failed", err);
            throw new Exception("Failed to communicate with audio processor: " + err.getMessage(), err);
        }

        AudioProcessorOuterClass.AudioInfoResponse response = responseRef.get();
        if (response == null) {
            throw new Exception("No audio info returned");
        }

        return response;
    }
    /**
     * Stream processed audio using async gRPC (no disk writes)
     */
    public void applyEffectsStream(String audioPath, float speedFactor, float pitchFactor, OutputStream outputStream) 
            throws IOException, Exception {
        log.info("Applying effects with streaming: speed={}, pitch={} for {}", speedFactor, pitchFactor, audioPath);
        
        AudioProcessorOuterClass.ApplyEffectsRequest request = AudioProcessorOuterClass.ApplyEffectsRequest.newBuilder()
                .setAudioPath(audioPath)
                .setSpeedFactor(speedFactor)
                .setPitchFactor(pitchFactor)
                .build();
        
        CountDownLatch completionLatch = new CountDownLatch(1);
        AtomicReference<Throwable> errorRef = new AtomicReference<>();
        int[] chunkCount = {0};
        
        StreamObserver<AudioProcessorOuterClass.AudioChunk> responseObserver = new StreamObserver<AudioProcessorOuterClass.AudioChunk>() {
            @Override
            public void onNext(AudioProcessorOuterClass.AudioChunk chunk) {
                try {
                    outputStream.write(chunk.getData().toByteArray());
                    chunkCount[0]++;
                } catch (IOException e) {
                    errorRef.set(e);
                    log.error("Failed to write chunk to output stream", e);
                }
            }

            @Override
            public void onError(Throwable t) {
                errorRef.set(t);
                completionLatch.countDown();
            }

            @Override
            public void onCompleted() {
                try {
                    outputStream.flush();
                } catch (IOException e) {
                    log.warn("Failed to flush output stream", e);
                }
                completionLatch.countDown();
            }
        };
        
        try {
            asyncStub.applyEffectsStream(request, responseObserver);
            
            if (!completionLatch.await(120, TimeUnit.SECONDS)) {
                throw new Exception("Audio stream timed out");
            }
            
            if (errorRef.get() != null) {
                throw new Exception("Stream error: " + errorRef.get().getMessage(), errorRef.get());
            }
            
            log.info("Effects applied and streamed successfully: {} chunks", chunkCount[0]);
            
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new Exception("Interrupted during stream", e);
        }
    }
}

