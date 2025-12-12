package com.soundboard.controller;

import com.soundboard.event.PlayEvent;
import com.soundboard.service.AudioProcessorService;
import com.soundboard.service.ClipService;
import com.soundboard.service.PlayEventProducer;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.io.InputStreamResource;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.UUID;

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
    public ResponseEntity<InputStreamResource> streamAudio(
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
            InputStream fileStream = Files.newInputStream(Paths.get(audioFilePath));
            return ResponseEntity.ok()
                    .contentType(MediaType.parseMediaType("audio/mpeg"))
                    .header(HttpHeaders.CONTENT_DISPOSITION, "inline")
                    .header(HttpHeaders.CACHE_CONTROL, "public, max-age=3600")
                    .body(new InputStreamResource(fileStream));
        }

        // Apply effects via gRPC to C++ service
        try {
            // Use temp filename in shared audio directory
            String outputPath = audioDir + "/temp_" + UUID.randomUUID() + ".mp3";
            log.info("Applying effects: speed={}, pitch={}, output={}", speed, pitch, outputPath);

            String processedPath = audioProcessorService.applyEffects(
                    audioFilePath, outputPath, (float) speed, (float) pitch);

            log.info("Effects applied successfully, serving: {}", processedPath);

            InputStream fileStream = Files.newInputStream(Paths.get(processedPath));

            return ResponseEntity.ok()
                    .contentType(MediaType.parseMediaType("audio/mpeg"))
                    .header(HttpHeaders.CONTENT_DISPOSITION, "inline")
                    .header(HttpHeaders.CACHE_CONTROL, "public, max-age=3600")
                    .body(new InputStreamResource(fileStream));

        } catch (Exception e) {
            log.error("Failed to apply audio effects for clip {}", id, e);
            return ResponseEntity.internalServerError().build();
        }
    }
}
