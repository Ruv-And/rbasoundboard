-- Create clips table
CREATE TABLE clips (
    id SERIAL PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    description TEXT,
    original_file_url VARCHAR(500) NOT NULL,
    audio_file_url VARCHAR(500),
    thumbnail_url VARCHAR(500),
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