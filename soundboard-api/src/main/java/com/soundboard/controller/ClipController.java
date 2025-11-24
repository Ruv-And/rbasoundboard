package com.soundboard.controller;

import com.soundboard.dto.ClipDTO;
import com.soundboard.dto.UploadResponse;
import com.soundboard.model.Clip;
import com.soundboard.service.ClipService;
import com.soundboard.service.StorageService;
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
@CrossOrigin(origins = {"http://localhost:3000", "http://localhost:5173"})
@Slf4j
public class ClipController {
    
    private final ClipService clipService;
    private final StorageService storageService;
    
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
    
    @PostMapping("/upload")
    public ResponseEntity<UploadResponse> uploadClip(
            @RequestParam("file") MultipartFile file,
            @RequestParam("title") String title,
            @RequestParam(value = "description", required = false) String description,
            @RequestParam(value = "uploadedBy", defaultValue = "anonymous") String uploadedBy) {
        
        log.info("POST /api/clips/upload - title: {}, file: {}", title, file.getOriginalFilename());
        
        try {
            // Save uploaded file
            String filePath = storageService.saveUpload(file);
            
            // Create clip entity
            Clip clip = new Clip();
            clip.setTitle(title);
            clip.setDescription(description);
            clip.setOriginalFileUrl(filePath);
            clip.setUploadedBy(uploadedBy);
            clip.setFileSizeBytes(file.getSize());
            clip.setIsProcessed(false);
            
            Clip savedClip = clipService.createClip(clip);
            
            // TODO: Trigger audio processing via gRPC
            
            UploadResponse response = new UploadResponse(
                savedClip.getId(),
                "pending",
                "File uploaded successfully, processing audio...",
                null
            );
            
            return ResponseEntity.status(HttpStatus.CREATED).body(response);
            
        } catch (Exception e) {
            log.error("Error uploading file", e);
            UploadResponse response = new UploadResponse(
                null,
                "error",
                "Failed to upload file: " + e.getMessage(),
                null
            );
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
}