package com.soundboard.service;

import com.soundboard.event.PlayEvent;
import lombok.RequiredArgsConstructor;
import lombok.extern.slf4j.Slf4j;
import org.springframework.kafka.core.KafkaTemplate;
import org.springframework.kafka.support.KafkaHeaders;
import org.springframework.messaging.Message;
import org.springframework.messaging.support.MessageBuilder;
import org.springframework.stereotype.Service;

@Service
@RequiredArgsConstructor
@Slf4j
public class PlayEventProducer {

    private final KafkaTemplate<String, PlayEvent> kafkaTemplate;
    private static final String TOPIC = "clip-play-events";

    /**
     * Publish a play event to Kafka asynchronously
     */
    public void publishPlayEvent(PlayEvent event) {
        try {
            Message<PlayEvent> message = MessageBuilder
                    .withPayload(event)
                    .setHeader(KafkaHeaders.TOPIC, TOPIC)
                    .setHeader(KafkaHeaders.MESSAGE_KEY, String.valueOf(event.getClipId()))
                    .build();

            kafkaTemplate.send(message).whenComplete((result, ex) -> {
                if (ex != null) {
                    log.error("Failed to publish play event for clip {}: {}", event.getClipId(), ex.getMessage());
                } else {
                    log.debug("Published play event for clip {} with partition offset: {}",
                            event.getClipId(),
                            result.getRecordMetadata().offset());
                }
            });
        } catch (Exception e) {
            log.error("Error publishing play event: {}", e.getMessage(), e);
        }
    }
}
