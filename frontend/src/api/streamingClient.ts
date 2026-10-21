import type { components } from "../types/generated/streaming";
import { request } from "./httpClient";

type StartUploadRequest = components["schemas"]["StartUploadRequest"];
type StartUploadResponse = components["schemas"]["StartUploadResponse"];
type UploadStatus = components["schemas"]["UploadStatus"];
type CompleteUploadRequest = components["schemas"]["CompleteUploadRequest"];
type ProcessingStatus = components["schemas"]["ProcessingStatus"];

const baseUrl = import.meta.env.VITE_STREAMING_API_URL ?? "http://localhost:8083/api/v1";

export const streamingClient = {
  startUpload: (videoId: string, body: StartUploadRequest) =>
    request<StartUploadResponse>(`${baseUrl}/videos/${videoId}/upload`, { method: "POST", body: JSON.stringify(body), requiresAuth: true }),
  uploadChunk: (uploadId: string, chunkNumber: number, bytes: Blob, checksum: string, contentRange: string) =>
    request<void>(`${baseUrl}/uploads/${uploadId}/chunks/${chunkNumber}`, {
      method: "PUT",
      headers: { "Content-Type": "application/octet-stream", "X-Chunk-Checksum": checksum, "Content-Range": contentRange },
      body: bytes, requiresAuth: true,
    }),
  getUpload: (uploadId: string) => request<UploadStatus>(`${baseUrl}/uploads/${uploadId}`, { requiresAuth: true }),
  completeUpload: (uploadId: string, body: CompleteUploadRequest) =>
    request<UploadStatus>(`${baseUrl}/uploads/${uploadId}/complete`, { method: "POST", body: JSON.stringify(body), requiresAuth: true }),
  cancelUpload: (uploadId: string) => request<void>(`${baseUrl}/uploads/${uploadId}`, { method: "DELETE", requiresAuth: true }),
  getProcessing: (videoId: string) => request<ProcessingStatus>(`${baseUrl}/videos/${videoId}/processing`, { requiresAuth: true }),
  getManifestUrl: (videoId: string) => `${baseUrl}/videos/${videoId}/manifest.mpd`,
  getThumbnail: (videoId: string) => request<Blob>(`${baseUrl}/videos/${videoId}/thumbnail`, { requiresAuth: true }),
};
