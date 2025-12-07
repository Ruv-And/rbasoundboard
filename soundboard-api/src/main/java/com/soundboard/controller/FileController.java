package com.soundboard.controller;

import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.io.Resource;
import org.springframework.core.io.UrlResource;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.nio.file.Path;
import java.nio.file.Paths;

@RestController
@RequestMapping("/api/files")
@CrossOrigin(origins = { "http://localhost:3000", "http://localhost:5173", "http://localhost" })
@Slf4j
public class FileController {

    @Value("${storage.upload-dir}")
    private String uploadDir;

    @Value("${storage.audio-dir}")
    private String audioDir;

    @Value("${storage.thumbnail-dir}")
    private String thumbnailDir;

    @GetMapping("/audio/{filename:.+}")
    public ResponseEntity<Resource> serveAudioFile(@PathVariable String filename) {
        try {
            log.info("Serving audio file: {}", filename);

            // Try upload dir first (where files are initially stored)
            Path uploadPath = Paths.get(uploadDir).resolve(filename);
            Resource resource = new UrlResource(uploadPath.toUri());

            if (!resource.exists()) {
                // Try audio dir
                Path audioPath = Paths.get(audioDir).resolve(filename);
                resource = new UrlResource(audioPath.toUri());
            }

            if (resource.exists() && resource.isReadable()) {
                String contentType = determineContentType(filename);

                return ResponseEntity.ok()
                        .contentType(MediaType.parseMediaType(contentType))
                        .header(HttpHeaders.CONTENT_DISPOSITION, "inline; filename=\"" + filename + "\"")
                        .body(resource);
            } else {
                log.error("File not found or not readable: {}", filename);
                return ResponseEntity.notFound().build();
            }
        } catch (Exception e) {
            log.error("Error serving file: {}", filename, e);
            return ResponseEntity.internalServerError().build();
        }
    }

    @GetMapping("/thumbnail/{filename:.+}")
    public ResponseEntity<Resource> serveThumbnailFile(@PathVariable String filename) {
        try {
            log.info("Serving thumbnail file: {}", filename);
            Path filePath = Paths.get(thumbnailDir).resolve(filename);
            Resource resource = new UrlResource(filePath.toUri());

            if (resource.exists() && resource.isReadable()) {
                return ResponseEntity.ok()
                        .contentType(MediaType.IMAGE_PNG)
                        .header(HttpHeaders.CONTENT_DISPOSITION, "inline; filename=\"" + filename + "\"")
                        .body(resource);
            } else {
                return ResponseEntity.notFound().build();
            }
        } catch (Exception e) {
            log.error("Error serving thumbnail: {}", filename, e);
            return ResponseEntity.internalServerError().build();
        }
    }

    private String determineContentType(String filename) {
        String lowerFilename = filename.toLowerCase();

        if (lowerFilename.endsWith(".mp4"))
            return "video/mp4";
        if (lowerFilename.endsWith(".mp3"))
            return "audio/mpeg";
        if (lowerFilename.endsWith(".wav"))
            return "audio/wav";
        if (lowerFilename.endsWith(".ogg"))
            return "audio/ogg";
        if (lowerFilename.endsWith(".webm"))
            return "video/webm";
        if (lowerFilename.endsWith(".avi"))
            return "video/x-msvideo";
        if (lowerFilename.endsWith(".mov"))
            return "video/quicktime";

        return "application/octet-stream";
    }
}
