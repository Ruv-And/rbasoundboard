package com.soundboard.service;

import com.soundboard.event.PlayEvent;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.kafka.annotation.KafkaListener;
import org.springframework.stereotype.Component;

import java.time.LocalDateTime;

@Component
@RequiredArgsConstructor
@Slf4j
public class PlayEventConsumer {

    private final PlayStatsService playStatsService;

    @KafkaListener(topics = "clip-play-events", groupId = "soundboard-api", containerFactory = "kafkaListenerContainerFactory")
    public void consume(PlayEvent event) {
        if (event == null || event.getClipId() == null) {
            log.warn("Received null or invalid play event: {}", event);
            return;
        }
        LocalDateTime playedAt = event.getTimestamp() != null ? event.getTimestamp() : LocalDateTime.now();
        log.info("Consumed play event for clip {} at {}", event.getClipId(), playedAt);
        playStatsService.incrementPlayCount(event.getClipId(), playedAt);
    }
}
