package com.soundboard.dto;

import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.math.BigDecimal;
import java.time.LocalDateTime;

@Data
@NoArgsConstructor
@AllArgsConstructor
public class ClipDTO {
    private Long id;
    private String title;
    private String description;
    private String audioUrl;
    private String thumbnailUrl;
    private BigDecimal durationSeconds;
    private Long fileSizeBytes;
    private String uploadedBy;
    private LocalDateTime uploadDate;
    private Boolean isProcessed;
}