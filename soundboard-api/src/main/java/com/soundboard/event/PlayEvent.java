package com.soundboard.event;

import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.time.LocalDateTime;

@Data
@NoArgsConstructor
@AllArgsConstructor
public class PlayEvent {

    @JsonProperty("clip_id")
    private Long clipId;

    @JsonProperty("timestamp")
    private LocalDateTime timestamp;

    @JsonProperty("user_id")
    private String userId;

    @JsonProperty("session_id")
    private String sessionId;

    @JsonProperty("speed")
    private Double speed;

    @JsonProperty("pitch")
    private Double pitch;

    public PlayEvent(Long clipId, String userId, Double speed, Double pitch) {
        this.clipId = clipId;
        this.userId = userId;
        this.speed = speed;
        this.pitch = pitch;
        this.timestamp = LocalDateTime.now();
    }
}
