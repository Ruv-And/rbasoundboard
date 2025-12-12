CREATE TABLE play_stats (
    id SERIAL PRIMARY KEY,
    clip_id BIGINT NOT NULL UNIQUE,
    play_count BIGINT NOT NULL DEFAULT 0,
    last_played TIMESTAMP,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (clip_id) REFERENCES clips(id) ON DELETE CASCADE
);

CREATE INDEX idx_play_stats_play_count ON play_stats(play_count DESC);
CREATE INDEX idx_play_stats_last_played ON play_stats(last_played DESC);
CREATE INDEX idx_play_stats_clip_id ON play_stats(clip_id);
