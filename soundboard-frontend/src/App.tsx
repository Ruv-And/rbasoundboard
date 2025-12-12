import React, { useState, useEffect } from "react";
import { clipService } from "./services/api";
import ClipCard from "./components/ClipCard";
import UploadModal from "./components/UploadModal";
import ColorBends from "./components/ColorBends";
import PrismaticBurst from "./components/Burst";
const App: React.FC = () => {
  const [clips, setClips] = useState<any[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);
  const [isUploadModalOpen, setIsUploadModalOpen] = useState<boolean>(false);
  const [currentAudio, setCurrentAudio] = useState<HTMLAudioElement | null>(
    null
  );

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
      console.error("Error loading clips:", err);
      setError("Failed to load clips. Make sure the API is running.");
    } finally {
      setLoading(false);
    }
  };

  const handlePlay = (clip: any, speed: number = 1.0, pitch: number = 1.0) => {
    if (!clip.audioUrl) {
      alert("Audio not available yet. The clip may still be processing.");
      return;
    }

    if (currentAudio) {
      currentAudio.pause();
    }

    // Always use streaming endpoint to ensure play tracking via Kafka
    // AudioStreamController will serve files directly when speed=pitch=1.0 (no C++ processing)
    const audioUrl = `/api/stream/${clip.id}?speed=${speed}&pitch=${pitch}`;

    const audio = new Audio(audioUrl);
    audio.play();
    setCurrentAudio(audio);

    console.log("Playing:", clip.title, "with speed:", speed, "pitch:", pitch);
  };

  const handleDelete = async (id: string | number) => {
    if (!window.confirm("Are you sure you want to delete this clip?")) {
      return;
    }

    try {
      await clipService.deleteClip(id);
      setClips(clips.filter((clip) => clip.id !== id));
    } catch (err) {
      console.error("Error deleting clip:", err);
      alert("Failed to delete clip");
    }
  };

  const handleUploadSuccess = () => {
    loadClips();
  };

  return (
    <div className="min-h-screen relative overflow-hidden bg-black">
      {/* Background */}
      <div className="fixed inset-0 z-0 bg-black w-full h-full">
        <ColorBends
          colors={["#49C867", "#34A853", "#13B1EC"]}
          rotation={0}
          speed={0.7}
          scale={0.4}
          frequency={1}
          warpStrength={1}
          mouseInfluence={0}
          parallax={1}
          noise={0.3}
        />
        {/* <PrismaticBurst
          animationType="rotate3d"
          intensity={2}
          speed={0.5}
          distort={0}
          paused={false}
          offset={{ x: 0, y: 0 }}
          hoverDampness={0}
          rayCount={0}
          mixBlendMode="lighten"
          colors={["#49C867", "#34A853", "#13B1EC"]}
        /> */}
      </div>

      {/* Content overlay */}
      <div className="relative z-10">
        {/* Backdrop blur when modal is open */}
        {isUploadModalOpen && (
          <div
            className="fixed inset-0 bg-black/20 backdrop-blur-sm z-30"
            onClick={() => setIsUploadModalOpen(false)}
          />
        )}

        <header className="bg-gray-900/80 backdrop-blur-md shadow-sm sticky top-0 z-40 border-b border-gray-700">
          <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-4">
            <div className="flex justify-between items-center">
              <div>
                <h1 className="text-2xl sm:text-3xl font-bold bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] bg-clip-text text-transparent">
                  RBA Soundboard
                </h1>
              </div>

              <button
                onClick={() => setIsUploadModalOpen(true)}
                className="px-4 sm:px-6 py-2 sm:py-3 bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] hover:from-[#49C867] hover:via-[#34A853] hover:to-[#13B1EC] text-white rounded-full font-medium transition-all shadow-lg hover:shadow-xl text-sm sm:text-base"
              >
                + Upload
              </button>
            </div>
          </div>
        </header>

        <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6">
          {loading && (
            <div className="text-center py-12">
              <div className="inline-block animate-spin rounded-full h-12 w-12 border-b-2 border-blue-400"></div>
              <p className="mt-4 text-gray-300">Loading clips...</p>
            </div>
          )}

          {error && (
            <div className="bg-red-900/30 border-l-4 border-red-500 p-4 mb-6 rounded-lg">
              <div className="flex">
                <div className="flex-shrink-0">
                  <svg
                    className="h-5 w-5 text-red-400"
                    viewBox="0 0 20 20"
                    fill="currentColor"
                  >
                    <path
                      fillRule="evenodd"
                      d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z"
                      clipRule="evenodd"
                    />
                  </svg>
                </div>
                <div className="ml-3">
                  <p className="text-sm text-red-200">{error}</p>
                </div>
              </div>
            </div>
          )}

          {!loading && clips.length === 0 && !error && (
            <div className="text-center py-12">
              <div className="text-6xl mb-4">🎤</div>
              <h2 className="text-2xl font-semibold text-gray-100 mb-2">
                No clips yet!
              </h2>
              <p className="text-gray-400 mb-6">
                Upload your first clip to get started.
              </p>
              <button
                onClick={() => setIsUploadModalOpen(true)}
                className="px-6 py-3 bg-gradient-to-r from-blue-500 to-purple-500 hover:from-blue-600 hover:to-purple-600 text-white rounded-xl font-medium transition-all shadow-lg"
              >
                Upload Your First Clip
              </button>
            </div>
          )}

          {!loading && clips.length > 0 && (
            <>
              <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-4 xl:grid-cols-5 gap-3 sm:gap-4">
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

        <UploadModal
          isOpen={isUploadModalOpen}
          onClose={() => setIsUploadModalOpen(false)}
          onUploadSuccess={handleUploadSuccess}
        />
      </div>
    </div>
  );
};

export default App;
