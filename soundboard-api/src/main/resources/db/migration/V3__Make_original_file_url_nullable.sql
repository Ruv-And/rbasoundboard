-- Make original_file_url nullable since audio files don't need it
ALTER TABLE clips
ALTER COLUMN original_file_url DROP NOT NULL;

