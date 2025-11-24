package com.soundboard.service;

import lombok.extern.slf4j.Slf4j;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.UUID;

@Service
@Slf4j
public class StorageService {
    
    @Value("${storage.upload-dir}")
    private String uploadDir;
    
    @Value("${storage.audio-dir}")
    private String audioDir;
    
    @Value("${storage.thumbnail-dir}")
    private String thumbnailDir;
    
    public String saveFile(MultipartFile file, String directory) throws IOException {
        // Create directory if it doesn't exist
        Path dirPath = Paths.get(directory);
        if (!Files.exists(dirPath)) {
            Files.createDirectories(dirPath);
        }
        
        // Generate unique filename
        String originalFilename = file.getOriginalFilename();
        String extension = originalFilename != null && originalFilename.contains(".") 
            ? originalFilename.substring(originalFilename.lastIndexOf("."))
            : "";
        String uniqueFilename = UUID.randomUUID().toString() + extension;
        
        // Save file
        Path filePath = dirPath.resolve(uniqueFilename);
        Files.copy(file.getInputStream(), filePath, StandardCopyOption.REPLACE_EXISTING);
        
        log.info("File saved: {}", filePath);
        return filePath.toString();
    }
    
    public String saveUpload(MultipartFile file) throws IOException {
        return saveFile(file, uploadDir);
    }
    
    public String saveAudio(MultipartFile file) throws IOException {
        return saveFile(file, audioDir);
    }
    
    public String saveThumbnail(MultipartFile file) throws IOException {
        return saveFile(file, thumbnailDir);
    }
    
    public void deleteFile(String filePath) throws IOException {
        Path path = Paths.get(filePath);
        if (Files.exists(path)) {
            Files.delete(path);
            log.info("File deleted: {}", filePath);
        }
    }
}