package com.soundboard.dto;

import com.fasterxml.jackson.annotation.JsonInclude;
import lombok.AllArgsConstructor;
import lombok.Data;
import lombok.NoArgsConstructor;

import java.time.LocalDateTime;

@Data
@NoArgsConstructor
@AllArgsConstructor
@JsonInclude(JsonInclude.Include.NON_NULL)
public class ClipDTO {
    private Long id;
    private String title;
    private String description;
    private String audioUrl;
    private String thumbnailUrl;
    private Long fileSizeBytes;
    private String uploadedBy;
    private LocalDateTime uploadDate;
    private Boolean isProcessed;
    private Long playCount;
    private LocalDateTime lastPlayed;
}