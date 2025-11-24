-- Insert sample tags
INSERT INTO tags (name) VALUES 
    ('funny'),
    ('epic'),
    ('fail'),
    ('reaction'),
    ('wtf'),
    ('legendary');

-- Insert sample clips
INSERT INTO clips (
    title, 
    description, 
    original_file_url, 
    audio_file_url, 
    thumbnail_url, 
    duration_seconds, 
    file_size_bytes, 
    uploaded_by, 
    is_processed
) VALUES 
    (
        'Epic Laugh', 
        'Johns legendary laugh moment', 
        '/uploads/laugh.mp4', 
        '/audio/laugh.mp3', 
        '/thumbnails/laugh.jpg', 
        5.2, 
        524288, 
        'demo_user', 
        true
    ),
    (
        'Chair Fail', 
        'Dave falling off his gaming chair', 
        '/uploads/fail.mp4', 
        '/audio/fail.mp3', 
        '/thumbnails/fail.jpg', 
        3.8, 
        389120, 
        'demo_user', 
        true
    ),
    (
        'Surprised Reaction', 
        'Sarahs priceless reaction', 
        '/uploads/reaction.mp4', 
        '/audio/reaction.mp3', 
        '/thumbnails/reaction.jpg', 
        2.5, 
        262144, 
        'demo_user', 
        true
    );

-- Link clips to tags
INSERT INTO clip_tags (clip_id, tag_id) VALUES
    (1, 1), -- Epic Laugh -> funny
    (1, 6), -- Epic Laugh -> legendary
    (2, 1), -- Chair Fail -> funny
    (2, 3), -- Chair Fail -> fail
    (3, 4); -- Surprised Reaction -> reaction