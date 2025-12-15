package com.soundboard.controller;

import com.soundboard.event.PlayEvent;
import com.soundboard.service.AudioProcessorService;
import com.soundboard.service.ClipService;
import com.soundboard.service.PlayEventProducer;
import com.soundboard.service.AudioStreamSession;
import com.soundboard.service.ProcessorBusyException;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.io.InputStreamResource;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Paths;

@RestController
@RequestMapping("/api/stream")
@RequiredArgsConstructor
@CrossOrigin(origins = { "http://localhost:3000", "http://localhost:5173" })
@Slf4j
public class AudioStreamController {

    private final ClipService clipService;
    private final AudioProcessorService audioProcessorService;
    private final PlayEventProducer playEventProducer;

    @Value("${storage.audio-dir}")
    private String audioDir;

    /**
     * Stream audio with optional speed and pitch modifications
     * Speed: 0.5 = half speed, 1.0 = normal, 2.0 = double speed
     * Pitch: 0.5 = half pitch (lower), 1.0 = normal, 2.0 = double pitch (higher)
     */
    @GetMapping("/{id}")
    public ResponseEntity<StreamingResponseBody> streamAudio(
            @PathVariable Long id,
            @RequestParam(defaultValue = "1.0") double speed,
            @RequestParam(defaultValue = "1.0") double pitch) throws IOException {

        log.info("Streaming audio for clip {} with speed={}, pitch={}", id, speed, pitch);

        // Get clip and audio file path
        var clipOpt = clipService.findById(id);
        if (clipOpt.isEmpty()) {
            log.warn("Clip {} not found", id);
            return ResponseEntity.notFound().build();
        }

        String audioFilePath = clipOpt.get().getAudioFileUrl();

        if (audioFilePath == null) {
            log.warn("No audio file for clip {}", id);
            return ResponseEntity.notFound().build();
        }

        // Publish play event to Kafka asynchronously (non-blocking)
        try {
            PlayEvent playEvent = new PlayEvent(id, "anonymous", speed, pitch);
            playEventProducer.publishPlayEvent(playEvent);
        } catch (Exception e) {
            log.error("Failed to publish play event for clip {}, continuing with stream", id, e);
            // Don't fail the request if Kafka is down
        }

        // If no modifications, serve file directly
        if (speed == 1.0 && pitch == 1.0) {
            log.info("Serving original file: {}", audioFilePath);
            StreamingResponseBody responseBody = outputStream -> {
                try (InputStream fileStream = Files.newInputStream(Paths.get(audioFilePath))) {
                    byte[] buffer = new byte[4096];
                    int bytesRead;
                    while ((bytesRead = fileStream.read(buffer)) != -1) {
                        outputStream.write(buffer, 0, bytesRead);
                    }
                    outputStream.flush();
                }
            };
            
            return ResponseEntity.ok()
                    .contentType(MediaType.parseMediaType("audio/mpeg"))
                    .header(HttpHeaders.CONTENT_DISPOSITION, "inline")
                    .header(HttpHeaders.CACHE_CONTROL, "public, max-age=3600")
                    .body(responseBody);
        }

        // Apply effects via gRPC to C++ service with streaming (no disk writes)
        try {
            log.info("Applying effects with streaming: speed={}, pitch={}", speed, pitch);
            // Probe to surface RESOURCE_EXHAUSTED before committing headers
            AudioStreamSession session = audioProcessorService.beginApplyEffectsStream(audioFilePath, (float) speed, (float) pitch);

            StreamingResponseBody responseBody = outputStream -> {
                try {
                    // Write first chunk if present
                    byte[] first = session.getFirstChunk();
                    if (first != null && first.length > 0) {
                        outputStream.write(first);
                    }
                    // Stream remaining chunks
                    var it = session.getIterator();
                    while (it.hasNext()) {
                        var chunk = it.next();
                        outputStream.write(chunk.getData().toByteArray());
                    }
                    outputStream.flush();
                } catch (Exception e) {
                    log.error("Failed to stream processed audio for clip {}", id, e);
                    throw new IOException("Failed to process audio: " + e.getMessage(), e);
                }
            };

            return ResponseEntity.ok()
                    .contentType(MediaType.parseMediaType("audio/mpeg"))
                    .header(HttpHeaders.CONTENT_DISPOSITION, "inline")
                    .header(HttpHeaders.CACHE_CONTROL, "public, max-age=3600")
                    .body(responseBody);

        } catch (ProcessorBusyException busy) {
            log.warn("Audio processor busy for clip {}: {}", id, busy.getMessage());
            return ResponseEntity.status(429)
                    .header("Retry-After", "2")
                    .build();
        } catch (Exception e) {
            log.error("Failed to stream processed audio for clip {}", id, e);
            return ResponseEntity.internalServerError().build();
        }
    }
}
