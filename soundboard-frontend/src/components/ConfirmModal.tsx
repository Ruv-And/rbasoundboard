import React, { useState, useEffect } from "react";

interface ConfirmModalProps {
  isOpen: boolean;
  title: string;
  message: string;
  onConfirm: (password?: string) => void;
  onCancel: () => void;
  confirmText?: string;
  cancelText?: string;
  requirePassword?: boolean;
}

const ConfirmModal: React.FC<ConfirmModalProps> = ({
  isOpen,
  title,
  message,
  onConfirm,
  onCancel,
  confirmText = "Confirm",
  cancelText = "Cancel",
  requirePassword = false,
}) => {
  const [password, setPassword] = useState("");

  useEffect(() => {
    if (!isOpen) {
      setPassword("");
    }
  }, [isOpen]);

  if (!isOpen) return null;

  const handleConfirm = () => {
    onConfirm(requirePassword ? password : undefined);
  };

  return (
    <div
      className="fixed inset-0 flex items-center justify-center z-50 p-4 bg-black/20 backdrop-blur-sm"
      onClick={onCancel}
    >
      <div
        className="bg-gray-900/80 backdrop-blur-md border border-gray-700 rounded-2xl p-6 w-full max-w-md shadow-2xl relative"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="mb-6">
          <h2 className="text-2xl font-bold bg-gradient-to-r from-[#49C867] via-[#34A853] to-[#13B1EC] bg-clip-text text-transparent mb-3">
            {title}
          </h2>
          <p className={`text-base ${message.includes("Incorrect") ? "text-red-400" : "text-gray-300"}`}>
            {message}
          </p>
          
          {requirePassword && (
            <div className="mt-4">
              <input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="Enter admin password"
                className={`w-full px-4 py-2.5 bg-gray-800/50 border rounded-lg text-white placeholder-gray-500 focus:outline-none focus:ring-2 transition-all ${
                  message.includes("Incorrect") 
                    ? "border-red-500 focus:ring-red-500/50 focus:border-red-500" 
                    : "border-gray-700 focus:ring-[#49C867]/50 focus:border-[#49C867]"
                }`}
                autoFocus
                onKeyDown={(e) => {
                  if (e.key === 'Enter' && password) {
                    handleConfirm();
                  }
                }}
              />
            </div>
          )}
        </div>

        <div className="flex gap-3">
          <button
            type="button"
            onClick={onCancel}
            className="flex-1 px-4 py-2.5 border border-gray-700 rounded-full hover:bg-gray-800 text-gray-200 font-medium transition-all"
          >
            {cancelText}
          </button>
          <button
            type="button"
            onClick={handleConfirm}
            disabled={requirePassword && !password}
            className="flex-1 px-4 py-2.5 bg-red-700/80 hover:bg-red-600 text-white rounded-full font-medium transition-all shadow-lg disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:bg-red-700/80"
          >
            {confirmText}
          </button>
        </div>
      </div>
    </div>
  );
};

export default ConfirmModal;
