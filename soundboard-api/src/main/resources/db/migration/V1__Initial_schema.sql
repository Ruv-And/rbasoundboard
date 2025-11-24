-- Create clips table
CREATE TABLE clips (
    id SERIAL PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    description TEXT,
    original_file_url VARCHAR(500) NOT NULL,
    audio_file_url VARCHAR(500),
    thumbnail_url VARCHAR(500),
    duration_seconds DECIMAL(10, 2),
    file_size_bytes BIGINT,
    uploaded_by VARCHAR(100),
    upload_date TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_processed BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create indexes
CREATE INDEX idx_clips_upload_date ON clips(upload_date DESC);
CREATE INDEX idx_clips_uploaded_by ON clips(uploaded_by);
CREATE INDEX idx_clips_is_processed ON clips(is_processed);

-- Create tags table
CREATE TABLE tags (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Create many-to-many relationship table
CREATE TABLE clip_tags (
    clip_id INTEGER REFERENCES clips(id) ON DELETE CASCADE,
    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (clip_id, tag_id)
);

-- Create index for faster tag lookups
CREATE INDEX idx_clip_tags_clip_id ON clip_tags(clip_id);
CREATE INDEX idx_clip_tags_tag_id ON clip_tags(tag_id);