package com.soundboard.model;

import jakarta.persistence.*;
import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.time.LocalDateTime;

@Entity
@Table(name = "play_stats")
@Data
@NoArgsConstructor
@AllArgsConstructor
public class PlayStats {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(name = "clip_id", nullable = false, unique = true)
    private Long clipId;

    @Column(name = "play_count", nullable = false)
    private Long playCount = 0L;

    @Column(name = "last_played")
    private LocalDateTime lastPlayed;

    @Column(name = "created_at")
    private LocalDateTime createdAt;

    @Column(name = "updated_at")
    private LocalDateTime updatedAt;

    @PrePersist
    protected void onCreate() {
        createdAt = LocalDateTime.now();
        updatedAt = LocalDateTime.now();
        playCount = 1L;
    }

    @PreUpdate
    protected void onUpdate() {
        updatedAt = LocalDateTime.now();
    }

    public PlayStats(Long clipId, LocalDateTime lastPlayed) {
        this.clipId = clipId;
        this.playCount = 1L;
        this.lastPlayed = lastPlayed;
    }
}
