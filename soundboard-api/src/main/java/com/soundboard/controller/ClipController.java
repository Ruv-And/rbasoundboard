package com.soundboard.controller;

import com.soundboard.dto.ClipDTO;
import com.soundboard.dto.UploadResponse;
import com.soundboard.model.Clip;
import com.soundboard.service.ClipService;
import com.soundboard.service.StorageService;
import com.soundboard.service.PlayStatsService;
import com.soundboard.service.UploadProcessingService;
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
    private final PlayStatsService playStatsService;
    private final UploadProcessingService uploadProcessingService;

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
            @RequestParam(value = "uploadedBy", defaultValue = "anonymous") String uploadedBy) {

        log.info("POST /api/clips/upload - title: {}, file: {}", title, file.getOriginalFilename());

        try {
            // Determine file type
            String originalFilename = file.getOriginalFilename();
            boolean isVideo = isVideoFile(originalFilename);
            boolean isMP3 = isMP3File(originalFilename);

            // Create clip entity
            Clip clip = new Clip();
            clip.setTitle(title);
            clip.setUploadedBy(uploadedBy);
            clip.setFileSizeBytes(file.getSize());
            clip.setIsProcessed(false);

            Clip savedClip = clipService.createClip(clip);

            // Process audio and save to audio directory
            String audioFilePath;
            if (isMP3) {
                // MP3 files: save directly to audio directory, no processing needed
                log.info("MP3 file detected, saving directly to audio directory");
                try {
                    audioFilePath = storageService.saveAudio(file);
                    savedClip.setAudioFileUrl(audioFilePath);
                    savedClip.setIsProcessed(true);
                    clipService.updateClip(savedClip.getId(), savedClip);
                    log.info("MP3 file saved directly: {}", audioFilePath);
                } catch (Exception e) {
                    log.error("Failed to save MP3 file", e);
                    clipService.deleteClip(savedClip.getId());
                    log.info("Deleted clip {} due to processing failure", savedClip.getId());
                    return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(new UploadResponse(
                            null,
                            "error",
                            "Failed to save MP3 file: " + e.getMessage(),
                            null));
                }
            } else {
                // Video or non-MP3 audio: save upload and process asynchronously via gRPC
                log.info("File requires processing (video or non-MP3); queuing async job");
                try {
                    String uploadedFilePath = storageService.saveUpload(file);
                    uploadProcessingService.processAsync(savedClip, uploadedFilePath);
                    log.info("Queued async processing for clip {} at {}", savedClip.getId(), uploadedFilePath);
                } catch (Exception e) {
                    log.error("Failed to enqueue processing job", e);
                    clipService.deleteClip(savedClip.getId());
                    log.info("Deleted clip {} due to enqueue failure", savedClip.getId());
                    return ResponseEntity.status(HttpStatus.BAD_REQUEST).body(new UploadResponse(
                            null,
                            "error",
                            "Failed to queue processing: " + e.getMessage(),
                            null));
                }
            }

            UploadResponse response = new UploadResponse(
                    savedClip.getId(),
                    savedClip.getIsProcessed() ? "success" : "processing",
                    savedClip.getIsProcessed() ? "File uploaded and processed successfully" : "File uploaded; processing in background",
                    null);

            if (savedClip.getIsProcessed()) {
                return ResponseEntity.status(HttpStatus.CREATED).body(response);
            }
            return ResponseEntity.status(HttpStatus.ACCEPTED).body(response);

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
                lowerFilename.endsWith(".mkv");
    }

    private boolean isMP3File(String filename) {
        if (filename == null)
            return false;
        return filename.toLowerCase().endsWith(".mp3");
    }
}