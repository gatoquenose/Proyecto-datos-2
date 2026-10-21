import type { components } from "../types/generated/metadata";
import { request } from "./httpClient";

type Video = components["schemas"]["Video"];
type VideoPage = components["schemas"]["VideoPage"];
type CreateVideoRequest = components["schemas"]["CreateVideoRequest"];
type UpdateVideoRequest = components["schemas"]["UpdateVideoRequest"];

const baseUrl = import.meta.env.VITE_METADATA_API_URL ?? "http://localhost:8082/api/v1";

export interface VideoListQuery {
  sortBy?: "title" | "author" | "uploadedAt";
  order?: "asc" | "desc";
  cursor?: string;
  pageSize?: number;
  q?: string;
  owner?: "me";
}

export const metadataClient = {
  createVideo: (body: CreateVideoRequest) =>
    request<Video>(`${baseUrl}/videos`, { method: "POST", body: JSON.stringify(body), requiresAuth: true }),
  listVideos: (query: VideoListQuery = {}) => {
    const params = new URLSearchParams();
    Object.entries(query).forEach(([key, value]) => {
      if (value !== undefined) params.set(key, String(value));
    });
    return request<VideoPage>(`${baseUrl}/videos?${params.toString()}`, { requiresAuth: true });
  },
  getVideo: (id: string) => request<Video>(`${baseUrl}/videos/${id}`, { requiresAuth: true }),
  updateVideo: (id: string, body: UpdateVideoRequest) =>
    request<Video>(`${baseUrl}/videos/${id}`, { method: "PATCH", body: JSON.stringify(body), requiresAuth: true }),
  updateVideoVisibility: (id: string, visibility: "public" | "private") =>
    request<Video>(`${baseUrl}/videos/${id}/visibility`, { method: "PATCH", body: JSON.stringify({ visibility }), requiresAuth: true }),
  deleteVideo: (id: string) => request<void>(`${baseUrl}/videos/${id}`, { method: "DELETE", requiresAuth: true }),
};
