import React from "react";

interface ClipCardProps {
  clip: any;
  onPlay: (clip: any) => void;
  onDelete: (id: string | number) => void;
}

const ClipCard: React.FC<ClipCardProps> = ({ clip, onPlay, onDelete }) => {
  return (
    <div className="bg-white rounded-lg shadow-md overflow-hidden hover:shadow-xl transition-shadow duration-300">
      <div className="aspect-video bg-gradient-to-br from-blue-100 to-purple-100 flex items-center justify-center relative">
        {clip.thumbnailUrl ? (
          <img
            src={clip.thumbnailUrl}
            alt={clip.title}
            className="w-full h-full object-cover"
          />
        ) : (
          <div className="text-6xl">🎵</div>
        )}

        {!clip.isProcessed && (
          <div className="absolute top-2 right-2 bg-yellow-400 text-yellow-900 text-xs px-2 py-1 rounded">
            Processing...
          </div>
        )}
      </div>

      <div className="p-4">
        <h3 className="font-bold text-lg mb-2 truncate text-gray-800">
          {clip.title}
        </h3>

        {clip.description && (
          <p className="text-gray-600 text-sm mb-3 line-clamp-2">
            {clip.description}
          </p>
        )}

        <div className="flex items-center justify-end text-sm text-gray-500 mb-3">
          <span>by {clip.uploadedBy || "anonymous"}</span>
        </div>

        <div className="flex gap-2">
          <button
            onClick={() => onPlay(clip)}
            disabled={!clip.isProcessed}
            className={`flex-1 px-4 py-2 rounded-lg font-medium transition-colors ${
              clip.isProcessed
                ? "bg-blue-500 hover:bg-blue-600 text-white"
                : "bg-gray-300 text-gray-500 cursor-not-allowed"
            }`}
          >
            ▶ Play
          </button>

          <button
            onClick={() => onDelete(clip.id)}
            className="px-4 py-2 bg-red-500 hover:bg-red-600 text-white rounded-lg font-medium transition-colors"
          >
            🗑️
          </button>
        </div>
      </div>
    </div>
  );
};

export default ClipCard;
