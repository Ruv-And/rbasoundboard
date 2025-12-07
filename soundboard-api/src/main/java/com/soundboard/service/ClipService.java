package com.soundboard.service;

import com.soundboard.dto.ClipDTO;
import com.soundboard.model.Clip;
import com.soundboard.repository.ClipRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.Optional;

@Service
@RequiredArgsConstructor
@Slf4j
public class ClipService {
    
    private final ClipRepository clipRepository;
    
    @Transactional(readOnly = true)
    public Page<ClipDTO> getAllClips(Pageable pageable) {
        log.debug("Fetching all clips with pagination: {}", pageable);
        return clipRepository.findAllByOrderByUploadDateDesc(pageable)
                .map(this::convertToDTO);
    }
    
    @Transactional(readOnly = true)
    public Optional<ClipDTO> getClipById(Long id) {
        log.debug("Fetching clip by id: {}", id);
        return clipRepository.findById(id)
                .map(this::convertToDTO);
    }
    
    @Transactional(readOnly = true)
    public Page<ClipDTO> searchClips(String query, Pageable pageable) {
        log.debug("Searching clips with query: {}", query);
        return clipRepository.searchClips(query, pageable)
                .map(this::convertToDTO);
    }
    
    @Transactional
    public Clip createClip(Clip clip) {
        log.info("Creating new clip: {}", clip.getTitle());
        return clipRepository.save(clip);
    }
    
    @Transactional
    public void deleteClip(Long id) {
        log.info("Deleting clip with id: {}", id);
        clipRepository.deleteById(id);
    }
    
    @Transactional
    public Optional<Clip> updateClip(Long id, Clip updatedClip) {
        log.info("Updating clip with id: {}", id);
        return clipRepository.findById(id)
                .map(clip -> {
                    clip.setTitle(updatedClip.getTitle());
                    clip.setDescription(updatedClip.getDescription());
                    clip.setAudioFileUrl(updatedClip.getAudioFileUrl());
                    clip.setThumbnailUrl(updatedClip.getThumbnailUrl());
                    clip.setIsProcessed(updatedClip.getIsProcessed());
                    return clipRepository.save(clip);
                });
    }
    
    private ClipDTO convertToDTO(Clip clip) {
        // Convert file path to URL path for frontend
        String audioUrl = null;
        if (clip.getAudioFileUrl() != null) {
            // Extract filename from path
            String fileName = clip.getAudioFileUrl().substring(
                clip.getAudioFileUrl().lastIndexOf('/') + 1
            );
            audioUrl = "/api/files/audio/" + fileName;
        }
        
        return new ClipDTO(
            clip.getId(),
            clip.getTitle(),
            clip.getDescription(),
            audioUrl,
            clip.getThumbnailUrl(),
            clip.getFileSizeBytes(),
            clip.getUploadedBy(),
            clip.getUploadDate(),
            clip.getIsProcessed()
        );
    }
}