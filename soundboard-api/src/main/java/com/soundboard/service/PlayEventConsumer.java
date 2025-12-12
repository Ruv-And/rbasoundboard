package com.soundboard.service;

import com.soundboard.event.PlayEvent;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Service;

import java.time.LocalDateTime;

@Service
@RequiredArgsConstructor
@Slf4j
public class PlayEventConsumer {

    private final PlayStatsService playStatsService;

    @KafkaListener(topics = "clip-play-events", groupId = "soundboard-consumer-group", concurrency = "3")
    public void consumePlayEvent(PlayEvent event) {
        try {
            log.debug("Consuming play event for clip: {}", event.getClipId());

            if (event.getClipId() == null) {
                log.warn("Received play event with null clipId");
                return;
            }

            // Update play statistics
            playStatsService.incrementPlayCount(event.getClipId(), LocalDateTime.now());

            log.debug("Successfully processed play event for clip: {}", event.getClipId());
        } catch (Exception e) {
            log.error("Error consuming play event: {}", e.getMessage(), e);
            // In production, you might want to send this to a dead-letter topic
        }
    }
}
