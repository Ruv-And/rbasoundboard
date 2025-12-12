package com.soundboard.service;

import com.soundboard.model.PlayStats;
import com.soundboard.repository.PlayStatsRepository;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.data.domain.Page;
import org.springframework.data.domain.Pageable;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.LocalDateTime;
import java.util.Optional;

@Service
@RequiredArgsConstructor
@Slf4j
public class PlayStatsService {

    private final PlayStatsRepository playStatsRepository;

    /**
     * Increment play count for a clip
     */
    @Transactional
    public void incrementPlayCount(Long clipId, LocalDateTime lastPlayed) {
        try {
            Optional<PlayStats> existingStats = playStatsRepository.findByClipId(clipId);

            if (existingStats.isPresent()) {
                PlayStats stats = existingStats.get();
                stats.setPlayCount(stats.getPlayCount() + 1);
                stats.setLastPlayed(lastPlayed);
                playStatsRepository.save(stats);
                log.debug("Incremented play count for clip: {}", clipId);
            } else {
                // Create new play stats entry
                PlayStats newStats = new PlayStats(clipId, lastPlayed);
                playStatsRepository.save(newStats);
                log.debug("Created new play stats entry for clip: {}", clipId);
            }
        } catch (Exception e) {
            log.error("Error incrementing play count for clip {}: {}", clipId, e.getMessage(), e);
        }
    }

    /**
     * Get play stats for a specific clip
     */
    public Optional<PlayStats> getPlayStats(Long clipId) {
        return playStatsRepository.findByClipId(clipId);
    }

    /**
     * Get all clips sorted by popularity (play count)
     */
    public Page<PlayStats> getPopularClips(Pageable pageable) {
        return playStatsRepository.findAllOrderByPlayCountDesc(pageable);
    }

    /**
     * Get play count for a clip
     */
    public Long getPlayCount(Long clipId) {
        return playStatsRepository.findByClipId(clipId)
                .map(PlayStats::getPlayCount)
                .orElse(0L);
    }
}
