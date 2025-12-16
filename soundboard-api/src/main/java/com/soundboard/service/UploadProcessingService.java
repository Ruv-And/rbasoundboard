package com.soundboard.service;

import jakarta.annotation.PreDestroy;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.stereotype.Service;

import com.soundboard.model.Clip;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

@Service
@RequiredArgsConstructor
@Slf4j
public class UploadProcessingService {

    private final AudioProcessorService audioProcessorService;
    private final StorageService storageService;
    private final ClipService clipService;

    private final ExecutorService executor = Executors.newFixedThreadPool(4);

    /**
     * Process uploads that need the C++ processor (video or non-mp3 audio) in the background.
     * Saves output, updates clip, and cleans up temp files. On failure, deletes the clip.
     */
    public void processAsync(Clip clip, String uploadedFilePath) {
        Long clipId = clip.getId();
        executor.submit(() -> {
            try {
                log.info("[AsyncUpload] Start processing clip {} from {}", clipId, uploadedFilePath);
                String audioFilePath = audioProcessorService.extractAudio(uploadedFilePath, "mp3", 192);
                storageService.deleteFile(uploadedFilePath);

                clip.setAudioFileUrl(audioFilePath);
                clip.setIsProcessed(true);
                clipService.updateClip(clipId, clip);
                log.info("[AsyncUpload] Completed clip {} -> {}", clipId, audioFilePath);
            } catch (Exception e) {
                log.error("[AsyncUpload] Failed processing clip {}: {}", clipId, e.getMessage(), e);
                try {
                    storageService.deleteFile(uploadedFilePath);
                } catch (Exception cleanup) {
                    log.warn("[AsyncUpload] Cleanup failed for {}: {}", uploadedFilePath, cleanup.getMessage());
                }
                try {
                    clipService.deleteClip(clipId);
                    log.info("[AsyncUpload] Deleted clip {} after failure", clipId);
                } catch (Exception deleteErr) {
                    log.warn("[AsyncUpload] Failed to delete clip {} after failure: {}", clipId, deleteErr.getMessage());
                }
            }
        });
    }

    @PreDestroy
    public void shutdown() {
        executor.shutdown();
    }
}
