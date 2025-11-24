import axios from 'axios';

const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080/api';

const api = axios.create({
  baseURL: API_BASE_URL,
  headers: {
    'Content-Type': 'application/json',
  },
});

export const clipService = {
  // Get all clips with pagination
  getClips: async (page = 0, size = 20) => {
    const response = await api.get('/clips', {
      params: { page, size }
    });
    return response.data;
  },

  // Get single clip
  getClip: async (id) => {
    const response = await api.get(`/clips/${id}`);
    return response.data;
  },

  // Search clips
  searchClips: async (query, page = 0, size = 20) => {
    const response = await api.get('/clips/search', {
      params: { q: query, page, size }
    });
    return response.data;
  },

  // Upload new clip
  uploadClip: async (file, title, description = '', uploadedBy = 'anonymous') => {
    const formData = new FormData();
    formData.append('file', file);
    formData.append('title', title);
    formData.append('description', description);
    formData.append('uploadedBy', uploadedBy);

    const response = await api.post('/clips/upload', formData, {
      headers: {
        'Content-Type': 'multipart/form-data',
      },
    });
    return response.data;
  },

  // Delete clip
  deleteClip: async (id) => {
    await api.delete(`/clips/${id}`);
  },
};

export default api;