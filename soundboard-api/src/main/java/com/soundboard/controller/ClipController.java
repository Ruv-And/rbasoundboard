package com.soundboard.controller;

import com.soundboard.dto.ClipDTO;
import com.soundboard.dto.UploadResponse;
import com.soundboard.model.Clip;
import com.soundboard.service.AudioProcessorService;
import com.soundboard.service.ClipService;
import com.soundboard.service.StorageService;
import com.soundboard.service.PlayStatsService;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageRequest;
import org.springframework.data.domain.Pageable;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.multipart.MultipartFile;

@RestController
@RequestMapping("/api/clips")
@RequiredArgsConstructor
@CrossOrigin(origins = { "http://localhost:3000", "http://localhost:5173" })
@Slf4j
public class ClipController {

    private final ClipService clipService;
    private final StorageService storageService;
    private final AudioProcessorService audioProcessorService;
    private final PlayStatsService playStatsService;

    @GetMapping
    public ResponseEntity<Page<ClipDTO>> getAllClips(
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        log.info("GET /api/clips - page: {}, size: {}", page, size);
        Pageable pageable = PageRequest.of(page, size);
        Page<ClipDTO> clips = clipService.getAllClips(pageable);
        return ResponseEntity.ok(clips);
    }

    @GetMapping("/{id}")
    public ResponseEntity<ClipDTO> getClipById(@PathVariable Long id) {
        log.info("GET /api/clips/{}", id);
        return clipService.getClipById(id)
                .map(ResponseEntity::ok)
                .orElse(ResponseEntity.notFound().build());
    }

    @GetMapping("/search")
    public ResponseEntity<Page<ClipDTO>> searchClips(
            @RequestParam String q,
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        log.info("GET /api/clips/search?q={}", q);
        Pageable pageable = PageRequest.of(page, size);
        Page<ClipDTO> clips = clipService.searchClips(q, pageable);
        return ResponseEntity.ok(clips);
    }

    @GetMapping("/popular")
    public ResponseEntity<Page<ClipDTO>> getPopularClips(
            @RequestParam(defaultValue = "0") int page,
            @RequestParam(defaultValue = "20") int size) {
        log.info("GET /api/clips/popular - page: {}, size: {}", page, size);
        Pageable pageable = PageRequest.of(page, size);
        Page<ClipDTO> clips = clipService.getPopularClips(pageable);
        return ResponseEntity.ok(clips);
    }

    @PostMapping("/upload")
    public ResponseEntity<UploadResponse> uploadClip(
            @RequestParam("file") MultipartFile file,
            @RequestParam("title") String title,
            @RequestParam(value = "description", required = false) String description,
            @RequestParam(value = "uploadedBy", defaultValue = "anonymous") String uploadedBy) {

        log.info("POST /api/clips/upload - title: {}, file: {}", title, file.getOriginalFilename());

        try {
            // Determine if this is a video file that needs audio extraction
            String originalFilename = file.getOriginalFilename();
            boolean isVideo = isVideoFile(originalFilename);

            // Create clip entity
            Clip clip = new Clip();
            clip.setTitle(title);
            clip.setDescription(description);
            clip.setUploadedBy(uploadedBy);
            clip.setFileSizeBytes(file.getSize());
            clip.setIsProcessed(false); // Will be set to true after processing

            Clip savedClip = clipService.createClip(clip);

            // Process audio and save to audio directory (always MP3)
            String audioFilePath;
            if (isVideo) {
                log.info("Video file detected, extracting audio via gRPC");
                try {
                    // Save video temporarily to uploads directory
                    String uploadedFilePath = storageService.saveUpload(file);
                    
                    // Extract audio as MP3 to audio directory
                    audioFilePath = audioProcessorService.extractAudio(uploadedFilePath, "mp3", 192);

                    // Delete original video file to save storage
                    storageService.deleteFile(uploadedFilePath);
                    log.info("Deleted original video file: {}", uploadedFilePath);

                    // Update clip with processed audio
                    savedClip.setAudioFileUrl(audioFilePath);
                    savedClip.setIsProcessed(true);
                    clipService.updateClip(savedClip.getId(), savedClip);

                    log.info("Audio extraction completed: {}", audioFilePath);
                } catch (Exception e) {
                    log.error("Failed to process audio", e);
                    // Keep clip as unprocessed, user can retry later
                    return ResponseEntity.status(HttpStatus.CREATED).body(new UploadResponse(
                            savedClip.getId(),
                            "error",
                            "File uploaded but audio processing failed: " + e.getMessage(),
                            null));
                }
            } else {
                // Audio file - transcode to MP3 in audio directory
                log.info("Audio file detected, transcoding to MP3 via gRPC");
                try {
                    // Save original audio temporarily to uploads directory
                    String uploadedFilePath = storageService.saveUpload(file);

                    // Convert to MP3 using the same extractAudio call (works for audio inputs too)
                    audioFilePath = audioProcessorService.extractAudio(uploadedFilePath, "mp3", 192);

                    // Delete original uploaded audio to save space
                    storageService.deleteFile(uploadedFilePath);
                    log.info("Deleted original audio file: {}", uploadedFilePath);

                    savedClip.setAudioFileUrl(audioFilePath);
                    savedClip.setIsProcessed(true);
                    clipService.updateClip(savedClip.getId(), savedClip);

                    log.info("Audio transcoding completed: {}", audioFilePath);
                } catch (Exception e) {
                    log.error("Failed to transcode audio", e);
                    return ResponseEntity.status(HttpStatus.CREATED).body(new UploadResponse(
                            savedClip.getId(),
                            "error",
                            "File uploaded but audio processing failed: " + e.getMessage(),
                            null));
                }
            }

            UploadResponse response = new UploadResponse(
                    savedClip.getId(),
                    "success",
                    "File uploaded and processed successfully",
                    null);

            return ResponseEntity.status(HttpStatus.CREATED).body(response);

        } catch (org.hibernate.exception.ConstraintViolationException e) {
            log.error("Database constraint violation during upload", e);
            UploadResponse response = new UploadResponse(
                    null,
                    "error",
                    "Invalid file data. Please ensure the file has proper metadata.",
                    null);
            return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(response);
        } catch (Exception e) {
            log.error("Error uploading file", e);
            UploadResponse response = new UploadResponse(
                    null,
                    "error",
                    "Failed to upload file: " + e.getMessage(),
                    null);
            return ResponseEntity.status(HttpStatus.INTERNAL_SERVER_ERROR).body(response);
        }
    }

    @DeleteMapping("/{id}")
    public ResponseEntity<Void> deleteClip(@PathVariable Long id) {
        log.info("DELETE /api/clips/{}", id);
        clipService.deleteClip(id);
        return ResponseEntity.noContent().build();
    }

    @GetMapping("/health")
    public ResponseEntity<String> health() {
        return ResponseEntity.ok("API is running!");
    }

    private boolean isVideoFile(String filename) {
        if (filename == null)
            return false;
        String lowerFilename = filename.toLowerCase();
        return lowerFilename.endsWith(".mp4") ||
                lowerFilename.endsWith(".avi") ||
                lowerFilename.endsWith(".mov") ||
                lowerFilename.endsWith(".webm") ||
                lowerFilename.endsWith(".mkv") ||
                lowerFilename.endsWith(".flv");
    }
}