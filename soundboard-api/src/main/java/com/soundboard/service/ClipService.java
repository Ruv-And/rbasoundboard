package com.soundboard.service;

import com.soundboard.dto.ClipDTO;
import com.soundboard.model.Clip;
import com.soundboard.model.PlayStats;
import com.soundboard.repository.ClipRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.PageImpl;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

@Service
@RequiredArgsConstructor
@Slf4j
public class ClipService {

    private final ClipRepository clipRepository;
    private final PlayStatsService playStatsService;

    @Transactional(readOnly = true)
    public Page<ClipDTO> getAllClips(Pageable pageable) {
        log.debug("Fetching all clips with pagination: {}", pageable);
        return clipRepository.findAllByOrderByUploadDateDesc(pageable)
                .map(this::convertToDTOWithStats);
    }

    @Transactional(readOnly = true)
    public Page<ClipDTO> getPopularClips(Pageable pageable) {
        log.debug("Fetching popular clips with pagination: {}", pageable);
        Page<PlayStats> popularStats = playStatsService.getPopularClips(pageable);

        // Get clip details for each stat
        List<ClipDTO> popularClips = popularStats.getContent().stream()
                .map(stat -> {
                    Optional<Clip> clip = clipRepository.findById(stat.getClipId());
                    if (clip.isPresent()) {
                        ClipDTO dto = convertToDTO(clip.get());
                        dto.setPlayCount(stat.getPlayCount());
                        dto.setLastPlayed(stat.getLastPlayed());
                        return dto;
                    }
                    return null;
                })
                .filter(dto -> dto != null)
                .collect(Collectors.toList());

        return new PageImpl<>(popularClips, pageable, popularStats.getTotalElements());
    }

    @Transactional(readOnly = true)
    public Optional<ClipDTO> getClipById(Long id) {
        log.debug("Fetching clip by id: {}", id);
        return clipRepository.findById(id)
                .map(this::convertToDTOWithStats);
    }

    @Transactional(readOnly = true)
    public Optional<Clip> findById(Long id) {
        return clipRepository.findById(id);
    }

    @Transactional(readOnly = true)
    public Page<ClipDTO> searchClips(String query, Pageable pageable) {
        log.debug("Searching clips with query: {}", query);
        return clipRepository.searchClips(query, pageable)
                .map(this::convertToDTOWithStats);
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
                    clip.getAudioFileUrl().lastIndexOf('/') + 1);
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
                clip.getIsProcessed(),
                null, // playCount - not included in basic conversion
                null // lastPlayed - not included in basic conversion
        );
    }

    private ClipDTO convertToDTOWithStats(Clip clip) {
        ClipDTO dto = convertToDTO(clip);

        // Add play statistics if available
        Optional<PlayStats> stats = playStatsService.getPlayStats(clip.getId());
        stats.ifPresent(playStats -> {
            dto.setPlayCount(playStats.getPlayCount());
            dto.setLastPlayed(playStats.getLastPlayed());
        });

        // Set play count to 0 if no stats exist
        if (dto.getPlayCount() == null) {
            dto.setPlayCount(0L);
        }

        return dto;
    }
}