import React, { useState, useRef, useEffect } from "react";

interface ClipCardProps {
  clip: any;
  onPlay: (clip: any, speed: number, pitch: number) => void;
  onDelete: (id: string | number) => void;
}

const ClipCard: React.FC<ClipCardProps> = ({ clip, onPlay, onDelete }) => {
  const [speed, setSpeed] = useState(1.0);
  const [pitch, setPitch] = useState(1.0);
  const [isExpanded, setIsExpanded] = useState(false);
  const [showEditTitle, setShowEditTitle] = useState(false);
  const [editTitle, setEditTitle] = useState(clip.title);
  const [isHolding, setIsHolding] = useState(false);
  const [holdProgress, setHoldProgress] = useState(0);
  const cardRef = useRef<HTMLDivElement>(null);
  const holdTimeoutRef = useRef<NodeJS.Timeout | null>(null);
  const holdStartRef = useRef<number | null>(null);
  const holdRafRef = useRef<number | null>(null);

  const HOLD_DURATION = 300; // ms for long-press

  // Handle click outside to close expanded view
  useEffect(() => {
    const handleClickOutside = (e: MouseEvent) => {
      if (cardRef.current && !cardRef.current.contains(e.target as Node)) {
        setIsExpanded(false);
      }
    };

    if (isExpanded) {
      document.addEventListener("mousedown", handleClickOutside);
      return () => {
        document.removeEventListener("mousedown", handleClickOutside);
      };
    }
  }, [isExpanded]);

  const startHold = () => {
    setIsHolding(true);
    setHoldProgress(0);
    holdStartRef.current = performance.now();

    const step = () => {
      if (holdStartRef.current == null) return;
      const elapsed = performance.now() - holdStartRef.current;
      const progress = Math.min(elapsed / HOLD_DURATION, 1);
      setHoldProgress(progress);
      if (progress >= 1) {
        cancelHold(false);
        setIsExpanded(true);
      } else {
        holdRafRef.current = requestAnimationFrame(step);
      }
    };

    holdRafRef.current = requestAnimationFrame(step);
  };

  const cancelHold = (resetProgress: boolean = true) => {
    setIsHolding(false);
    if (resetProgress) setHoldProgress(0);
    holdStartRef.current = null;
    if (holdRafRef.current) {
      cancelAnimationFrame(holdRafRef.current);
      holdRafRef.current = null;
    }
    if (holdTimeoutRef.current) {
      clearTimeout(holdTimeoutRef.current);
      holdTimeoutRef.current = null;
    }
  };

  const handleMouseDown = () => {
    startHold();
  };

  const handleMouseUp = () => {
    cancelHold();
  };

  const handleTouchStart = () => {
    startHold();
  };

  const handleTouchEnd = () => {
    cancelHold();
  };

  const handlePlay = () => {
    onPlay(clip, speed, pitch);
  };

  const handleReset = () => {
    setSpeed(1.0);
    setPitch(1.0);
  };

  const handleDelete = () => {
    onDelete(clip.id);
  };

  const handleTitleSave = () => {
    // Update the clip title locally (session state)
    clip.title = editTitle;
    setShowEditTitle(false);
  };

  // Expanded card view
  if (isExpanded) {
    return (
      <div
        ref={cardRef}
        className="bg-gray-900/80 backdrop-blur-md border border-gray-700 rounded-2xl shadow-lg p-3 space-y-3 text-gray-100"
      >
        {/* Title with Edit */}
        {showEditTitle ? (
          <div className="flex gap-2">
            <input
              type="text"
              value={editTitle}
              onChange={(e) => setEditTitle(e.target.value)}
              className="flex-1 px-2 py-1 border border-gray-700 rounded-md text-xs bg-gray-800 text-gray-100 focus:outline-none"
              autoFocus
            />
            <button
              onClick={handleTitleSave}
              className="px-2 py-1 text-sm rounded-full bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] hover:from-[#49C867] hover:via-[#34A853] hover:to-[#13B1EC] text-white"
            >
              ✓
            </button>
          </div>
        ) : (
          <div className="flex items-center justify-between">
            <h3 className="text-sm font-bold text-gray-100 truncate">
              {clip.title}
            </h3>
            <button
              onClick={() => setShowEditTitle(true)}
              className="text-gray-400 hover:text-gray-200 flex-shrink-0"
              title="Edit title"
            >
              ✏️
            </button>
          </div>
        )}

        {/* Speed Control */}
        <div className="space-y-1">
          <div className="flex justify-between items-center">
            <label className="text-xs font-medium text-gray-300">Speed</label>
            <span className="text-xs text-gray-300">{speed.toFixed(1)}x</span>
          </div>
          <input
            type="range"
            min="0.5"
            max="2.0"
            step="0.1"
            value={speed}
            onChange={(e) => setSpeed(parseFloat(e.target.value))}
            className="w-full h-1.5 bg-gray-700 rounded appearance-none cursor-pointer"
          />
        </div>

        {/* Pitch Control */}
        <div className="space-y-1">
          <div className="flex justify-between items-center">
            <label className="text-xs font-medium text-gray-300">Pitch</label>
            <span className="text-xs text-gray-300">{pitch.toFixed(1)}x</span>
          </div>
          <input
            type="range"
            min="0.5"
            max="2.0"
            step="0.1"
            value={pitch}
            onChange={(e) => setPitch(parseFloat(e.target.value))}
            className="w-full h-1.5 bg-gray-700 rounded appearance-none cursor-pointer"
          />
        </div>

        {/* Action Buttons */}
        <div className="flex gap-2 pt-1">
          <button
            onClick={handlePlay}
            disabled={!clip.isProcessed}
            className={`flex-1 py-1.5 rounded-full text-sm transition-colors ${
              clip.isProcessed
                ? "bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] hover:from-[#49C867] hover:via-[#34A853] hover:to-[#13B1EC] text-white"
                : "bg-gray-800 text-gray-500 cursor-not-allowed"
            }`}
          >
            ▶
          </button>

          <button
            onClick={handleReset}
            className="flex-1 py-1.5 bg-gray-800 hover:bg-gray-700 text-gray-200 rounded-full text-sm transition-colors"
          >
            ↺
          </button>

          <button
            onClick={handleDelete}
            className="flex-1 py-1.5 bg-red-700/80 hover:bg-red-600 text-white rounded-full text-sm transition-colors"
          >
            🗑
          </button>
        </div>
      </div>
    );
  }

  // Compact default view
  return (
    <div
      ref={cardRef}
      onMouseDown={handleMouseDown}
      onMouseUp={handleMouseUp}
      onMouseLeave={handleMouseUp}
      onTouchStart={handleTouchStart}
      onTouchEnd={handleTouchEnd}
      className="bg-gray-900/80 backdrop-blur-md border border-gray-700 rounded-2xl shadow-lg overflow-hidden hover:shadow-xl transition-all duration-200 cursor-pointer select-none text-gray-100"
    >
      {/* Thumbnail - Click to Play */}
      <div
        onClick={handlePlay}
        className="aspect-square relative flex items-center justify-center hover:opacity-95 transition-opacity"
      >
        {clip.thumbnailUrl ? (
          <img
            src={clip.thumbnailUrl}
            alt={clip.title}
            className="w-full h-full object-cover"
          />
        ) : (
          <div className="text-4xl">▶</div>
        )}

        {!clip.isProcessed && (
          <div className="absolute top-2 right-2 bg-yellow-400 text-yellow-900 text-xs px-2 py-1 rounded-full font-medium">
            Processing
          </div>
        )}

        {/* Holding progress indicator */}
        {isHolding && (
          <div className="absolute top-0 left-0 right-0 h-0.5">
            <div
              className="h-full bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC]"
              style={{ width: `${holdProgress * 100}%` }}
            />
          </div>
        )}

        {/* Gradient overlay bottom title bar with author */}
        <div className="absolute inset-x-0 bottom-0">
          <div className="px-3 py-2 bg-gray-900/80 backdrop-blur-md border-t border-gray-700">
            <h3 className="font-bold text-sm truncate bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] bg-clip-text text-transparent">
              {clip.title}
            </h3>
            <p className="text-xs pl-1 text-gray-500 truncate">
              {clip.uploadedBy || "Unknown"}
            </p>
          </div>
        </div>
      </div>
    </div>
  );
};

export default ClipCard;
