import React, { useState, useEffect } from 'react';
import { clipService } from './services/api';
import ClipCard from './components/ClipCard';
import UploadModal from './components/UploadModal';

function App() {
  const [clips, setClips] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [isUploadModalOpen, setIsUploadModalOpen] = useState(false);
  const [currentAudio, setCurrentAudio] = useState(null);

  useEffect(() => {
    loadClips();
  }, []);

  const loadClips = async () => {
    try {
      setLoading(true);
      const data = await clipService.getClips();
      setClips(data.content || []);
      setError(null);
    } catch (err) {
      console.error('Error loading clips:', err);
      setError('Failed to load clips. Make sure the API is running.');
    } finally {
      setLoading(false);
    }
  };

  const handlePlay = (clip) => {
    if (!clip.audioUrl) {
      alert('Audio not available yet. The clip may still be processing.');
      return;
    }
    
    // Stop current audio if playing
    if (currentAudio) {
      currentAudio.pause();
    }
    
    // Create and play new audio
    const audio = new Audio(clip.audioUrl);
    audio.play();
    setCurrentAudio(audio);
    
    console.log('Playing:', clip.title);
  };

  const handleDelete = async (id) => {
    if (!window.confirm('Are you sure you want to delete this clip?')) {
      return;
    }
    
    try {
      await clipService.deleteClip(id);
      setClips(clips.filter(clip => clip.id !== id));
    } catch (err) {
      console.error('Error deleting clip:', err);
      alert('Failed to delete clip');
    }
  };

  const handleUploadSuccess = () => {
    loadClips(); // Reload clips after upload
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-gray-50 to-gray-100">
      {/* Header */}
      <header className="bg-white shadow-sm sticky top-0 z-40">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-4">
          <div className="flex justify-between items-center">
            <div>
              <h1 className="text-3xl font-bold text-gray-900">
                🎵 Soundboard
              </h1>
              <p className="text-sm text-gray-500 mt-1">
                Your collection of funny moments
              </p>
            </div>
            
            <button
              onClick={() => setIsUploadModalOpen(true)}
              className="px-6 py-3 bg-blue-500 hover:bg-blue-600 text-white rounded-lg font-medium transition-colors shadow-md hover:shadow-lg"
            >
              + Upload Clip
            </button>
          </div>
        </div>
      </header>

      {/* Main Content */}
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {/* Loading State */}
        {loading && (
          <div className="text-center py-12">
            <div className="inline-block animate-spin rounded-full h-12 w-12 border-b-2 border-blue-500"></div>
            <p className="mt-4 text-gray-600">Loading clips...</p>
          </div>
        )}

        {/* Error State */}
        {error && (
          <div className="bg-red-50 border-l-4 border-red-500 p-4 mb-6">
            <div className="flex">
              <div className="flex-shrink-0">
                <svg className="h-5 w-5 text-red-400" viewBox="0 0 20 20" fill="currentColor">
                  <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
                </svg>
              </div>
              <div className="ml-3">
                <p className="text-sm text-red-700">{error}</p>
              </div>
            </div>
          </div>
        )}

        {/* Empty State */}
        {!loading && clips.length === 0 && !error && (
          <div className="text-center py-12">
            <div className="text-6xl mb-4">🎤</div>
            <h2 className="text-2xl font-semibold text-gray-700 mb-2">
              No clips yet!
            </h2>
            <p className="text-gray-500 mb-6">
              Upload your first funny moment to get started.
            </p>
            <button
              onClick={() => setIsUploadModalOpen(true)}
              className="px-6 py-3 bg-blue-500 hover:bg-blue-600 text-white rounded-lg font-medium transition-colors"
            >
              Upload Your First Clip
            </button>
          </div>
        )}

        {/* Clips Grid */}
        {!loading && clips.length > 0 && (
          <>
            <div className="mb-6">
              <h2 className="text-xl font-semibold text-gray-800">
                {clips.length} clip{clips.length !== 1 ? 's' : ''}
              </h2>
            </div>
            
            <div className="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 xl:grid-cols-4 gap-6">
              {clips.map((clip) => (
                <ClipCard
                  key={clip.id}
                  clip={clip}
                  onPlay={handlePlay}
                  onDelete={handleDelete}
                />
              ))}
            </div>
          </>
        )}
      </main>

      {/* Upload Modal */}
      <UploadModal
        isOpen={isUploadModalOpen}
        onClose={() => setIsUploadModalOpen(false)}
        onUploadSuccess={handleUploadSuccess}
      />
    </div>
  );
}

export default App;