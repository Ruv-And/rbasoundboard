import axios, { AxiosInstance } from "axios";

const API_BASE_URL: string =
  (import.meta.env.VITE_API_URL as string) || "http://localhost:8080/api";

const api: AxiosInstance = axios.create({
  baseURL: API_BASE_URL,
});

// Add a request interceptor to set Content-Type for non-FormData requests
api.interceptors.request.use((config) => {
  if (!(config.data instanceof FormData)) {
    config.headers["Content-Type"] = "application/json";
  }
  return config;
});

export const clipService = {
  getClips: async (page = 0, size = 20): Promise<any> => {
    const response = await api.get("/clips", { params: { page, size } });
    return response.data;
  },

  getClip: async (id: string | number): Promise<any> => {
    const response = await api.get(`/clips/${id}`);
    return response.data;
  },

  searchClips: async (query: string, page = 0, size = 20): Promise<any> => {
    const response = await api.get("/clips/search", {
      params: { q: query, page, size },
    });
    return response.data;
  },

  getPopularClips: async (page = 0, size = 20): Promise<any> => {
    const response = await api.get("/clips/popular", {
      params: { page, size },
    });
    return response.data;
  },

  uploadClip: async (
    file: File,
    title: string,
    description = "",
    uploadedBy = "anonymous"
  ): Promise<any> => {
    const formData = new FormData();
    formData.append("file", file);
    formData.append("title", title);
    formData.append("description", description);
    formData.append("uploadedBy", uploadedBy);

    const response = await api.post("/clips/upload", formData);
    return response.data;
  },

  deleteClip: async (id: string | number): Promise<void> => {
    await api.delete(`/clips/${id}`);
  },
};

export default api;
