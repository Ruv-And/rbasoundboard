import React, { useState, useEffect } from "react";
import { clipService } from "./services/api";
import ClipCard from "./components/ClipCard";
import UploadModal from "./components/UploadModal";
import ConfirmModal from "./components/ConfirmModal";
import GradientBackground from "./components/GradientBackground";

// Hide scrollbar globally
const scrollbarHideStyle = `
  ::-webkit-scrollbar {
    display: none;
  }
  html {
    scrollbar-width: none;
  }
`;

const App: React.FC = () => {
  const [clips, setClips] = useState<any[]>([]);
  const [loading, setLoading] = useState<boolean>(true);
  const [error, setError] = useState<string | null>(null);
  const [isUploadModalOpen, setIsUploadModalOpen] = useState<boolean>(false);
  const [isConfirmModalOpen, setIsConfirmModalOpen] = useState<boolean>(false);
  const [clipToDelete, setClipToDelete] = useState<string | number | null>(
    null
  );
  const [currentAudio, setCurrentAudio] = useState<HTMLAudioElement | null>(
    null
  );
  const [sortBy, setSortBy] = useState<"recent" | "popular">("popular");
  const [passwordError, setPasswordError] = useState<string>("");

  useEffect(() => {
    loadClips();
  }, [sortBy]);

  const loadClips = async () => {
    try {
      setLoading(true);
      const data =
        sortBy === "popular"
          ? await clipService.getPopularClips()
          : await clipService.getClips();
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

  const handleDelete = (id: string | number) => {
    setClipToDelete(id);
    setPasswordError("");
    setIsConfirmModalOpen(true);
  };

  const confirmDelete = async (password?: string) => {
    if (!clipToDelete || !password) return;

    try {
      await clipService.deleteClip(clipToDelete, password);
      setClips(clips.filter((clip) => clip.id !== clipToDelete));
      setIsConfirmModalOpen(false);
      setClipToDelete(null);
      setPasswordError("");
    } catch (err: any) {
      console.error("Error deleting clip:", err);
      
      // Check if it's an authentication error
      if (err.response?.status === 401 || err.response?.status === 403) {
        setPasswordError("Incorrect password");
      } else {
        alert("Failed to delete clip");
        setIsConfirmModalOpen(false);
        setClipToDelete(null);
        setPasswordError("");
      }
    }
  };

  const cancelDelete = () => {
    setIsConfirmModalOpen(false);
    setClipToDelete(null);
    setPasswordError("");
  };

  const handleUploadSuccess = () => {
    loadClips();
  };

  return (
    <>
      <style>{scrollbarHideStyle}</style>
      <div
        className="min-h-screen relative overflow-hidden bg-black"
        style={{ scrollbarWidth: "none", msOverflowStyle: "none" }}
      >
        {/* Background */}
        <div className="fixed inset-0 z-0 bg-black w-full h-full">
          <GradientBackground />
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

          <header className="bg-gray-950/95 backdrop-blur-md shadow-md fixed top-0 left-0 right-0 z-40 border-b border-gray-800">
            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-3">
              {/* Title and upload button row */}
              <div className="flex justify-between items-center mb-2">
                <h1 className="text-3xl sm:text-4xl font-bold bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] bg-clip-text text-transparent">
                  RBA Soundboard
                </h1>

                <button
                  onClick={() => setIsUploadModalOpen(true)}
                  className="px-4 sm:px-6 py-2 sm:py-2.5 bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] hover:from-[#49C867] hover:via-[#34A853] hover:to-[#13B1EC] text-white rounded-full font-medium transition-all shadow-lg hover:shadow-xl text-xs sm:text-sm"
                >
                  + Upload
                </button>
              </div>

              {/* Segmented control for sort*/}
              <div className="ml-1">
                <div className="inline-flex bg-gray-800/50 rounded-full p-1 border border-gray-700/50">
                  <button
                    onClick={() => setSortBy("popular")}
                    className={`px-4 py-1.5 rounded-full font-medium transition-all text-xs ${
                      sortBy === "popular"
                        ? "bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] text-white shadow-md"
                        : "text-gray-400 hover:text-gray-200"
                    }`}
                  >
                    Popular
                  </button>
                  <button
                    onClick={() => setSortBy("recent")}
                    className={`px-4 py-1.5 rounded-full font-medium transition-all text-xs ${
                      sortBy === "recent"
                        ? "bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] text-white shadow-md"
                        : "text-gray-400 hover:text-gray-200"
                    }`}
                  >
                    Recent
                  </button>
                </div>
              </div>
            </div>
          </header>

          <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-6 pt-[120px]">
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
                  className="px-6 py-3 bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] hover:from-[#49C867] hover:via-[#34A853] hover:to-[#13B1EC] text-white rounded-full font-medium transition-all shadow-lg"
                >
                  Upload Your First Clip
                </button>
              </div>
            )}

            {!loading && clips.length > 0 && (
              <>
                <div className="grid grid-cols-3 sm:grid-cols-4 lg:grid-cols-5 xl:grid-cols-6 gap-2 sm:gap-3">
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

          <ConfirmModal
            isOpen={isConfirmModalOpen}
            title="Delete Clip"
            message={passwordError || "Are you sure you want to delete this clip? This action cannot be undone."}
            onConfirm={confirmDelete}
            onCancel={cancelDelete}
            confirmText="Delete"
            cancelText="Cancel"
            requirePassword={true}
          />
        </div>
      </div>
    </>
  );
};

export default App;
