import React, { useState } from "react";
import { clipService } from "../services/api";

interface UploadModalProps {
  isOpen: boolean;
  onClose: () => void;
  onUploadSuccess: () => void;
}

const UploadModal: React.FC<UploadModalProps> = ({
  isOpen,
  onClose,
  onUploadSuccess,
}) => {
  const [file, setFile] = useState<File | null>(null);
  const [title, setTitle] = useState<string>("");
  const [description, setDescription] = useState<string>("");
  const [uploadedBy, setUploadedBy] = useState<string>("");
  const [uploading, setUploading] = useState<boolean>(false);
  const [error, setError] = useState<string>("");

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const selectedFile = e.target.files?.[0] ?? null;
    if (selectedFile) {
      setFile(selectedFile);
      if (!title) {
        const name = selectedFile.name.replace(/\.[^/.]+$/, "");
        setTitle(name);
      }
    }
  };

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault();

    if (!file || !title) {
      setError("Please select a file and enter a title");
      return;
    }

    setUploading(true);
    setError("");

    try {
      await clipService.uploadClip(
        file,
        title,
        description,
        uploadedBy || "anonymous"
      );

      setFile(null);
      setTitle("");
      setDescription("");
      setUploadedBy("");

      onUploadSuccess();
      onClose();
    } catch (err: any) {
      console.error("Upload error:", err);
      setError(err?.response?.data?.message || "Failed to upload file");
    } finally {
      setUploading(false);
    }
  };

  if (!isOpen) return null;

  return (
    <div
      className="fixed inset-0 flex items-center justify-center z-50 p-4"
      onClick={onClose}
    >
      <div
        className="bg-gray-900/80 backdrop-blur-md border border-gray-700 rounded-2xl p-6 w-full max-w-md shadow-2xl relative"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex justify-between items-center mb-6">
          <h2 className="text-2xl font-bold bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] bg-clip-text text-transparent">
            Upload Clip
          </h2>
          <button
            onClick={onClose}
            className="w-8 h-8 flex items-center justify-center rounded-full bg-gray-800 hover:bg-gray-700 text-gray-400 hover:text-gray-200 transition-all"
            title="Close"
          >
            ×
          </button>
        </div>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div>
            <label className="block text-sm font-semibold text-gray-200 mb-2">
              Video/Audio File *
            </label>
            <input
              type="file"
              accept="video/*,audio/*"
              onChange={handleFileChange}
              className="w-full px-4 py-2.5 border border-gray-700 rounded-xl bg-gray-800 text-gray-200 focus:outline-none focus:ring-2 focus:ring-[#34A853] focus:border-transparent transition-all"
              required
            />
            {file && (
              <p className="mt-2 text-sm text-gray-400 font-medium">
                📎 {file.name} ({(file.size / 1024 / 1024).toFixed(2)} MB)
              </p>
            )}
          </div>

          <div>
            <label className="block text-sm font-semibold text-gray-200 mb-2">
              Title *
            </label>
            <input
              type="text"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              className="w-full px-4 py-2.5 border border-gray-700 rounded-xl bg-gray-800 text-gray-200 focus:outline-none focus:ring-2 focus:ring-[#34A853] focus:border-transparent transition-all"
              placeholder="My funny moment"
              required
            />
          </div>

          <div>
            <label className="block text-sm font-semibold text-gray-200 mb-2">
              Your Name
            </label>
            <input
              type="text"
              value={uploadedBy}
              onChange={(e) => setUploadedBy(e.target.value)}
              className="w-full px-4 py-2.5 border border-gray-700 rounded-xl bg-gray-800 text-gray-200 focus:outline-none focus:ring-2 focus:ring-[#34A853] focus:border-transparent transition-all"
              placeholder="anonymous"
            />
          </div>

          {error && (
            <div className="p-3 bg-red-900/30 border border-red-700 rounded-xl text-red-200 text-sm font-medium">
              {error}
            </div>
          )}

          <div className="flex gap-3 pt-2">
            <button
              type="button"
              onClick={onClose}
              className="flex-1 px-4 py-2.5 border border-gray-700 rounded-full hover:bg-gray-800 text-gray-200 font-medium transition-all"
              disabled={uploading}
            >
              Cancel
            </button>
            <button
              type="submit"
              className="flex-1 px-4 py-2.5 bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] hover:from-[#49C867] hover:via-[#34A853] hover:to-[#13B1EC] text-white rounded-full font-medium transition-all shadow-lg disabled:opacity-50 disabled:cursor-not-allowed"
              disabled={uploading}
            >
              {uploading ? "Uploading..." : "Upload"}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
};

export default UploadModal;
