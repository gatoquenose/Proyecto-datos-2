import { useCallback, useEffect, useReducer, useRef } from "react";
import { useQueryClient } from "@tanstack/react-query";
import { ApiError } from "../types/api";
import type { components as MetadataComponents } from "../types/generated/metadata";
import type { components as StreamingComponents } from "../types/generated/streaming";
import { hashFileIncrementally, DEFAULT_CHUNK_SIZE_BYTES } from "../utils/checksum";

type CreateVideoRequest = MetadataComponents["schemas"]["CreateVideoRequest"];
type Video = MetadataComponents["schemas"]["Video"];
type StartUploadResponse = StreamingComponents["schemas"]["StartUploadResponse"];
type UploadStatus = StreamingComponents["schemas"]["UploadStatus"];
type ProcessingStatus = StreamingComponents["schemas"]["ProcessingStatus"];

export const UPLOAD_SESSION_KEY = "tecflix.pending-upload";
export const MAX_UPLOAD_ATTEMPTS = 3;
export const UPLOAD_RETRY_BASE_MS = 250;
export const DEFAULT_UPLOAD_POLL_MS = 2000;
export const DEFAULT_UPLOAD_CHUNK_SIZE = DEFAULT_CHUNK_SIZE_BYTES;

export interface UploadClients {
  createVideo: (body: CreateVideoRequest) => Promise<Video>;
  startUpload: (videoId: string, body: {
    fileName: string; contentType: string; totalBytes: number; chunkSize: number;
  }) => Promise<StartUploadResponse>;
  uploadChunk: (uploadId: string, chunkNumber: number, bytes: Blob, checksum: string, contentRange: string, options?: { signal?: AbortSignal }) => Promise<void>;
  getUpload: (uploadId: string) => Promise<UploadStatus>;
  completeUpload: (uploadId: string, body: { fileChecksum: string }) => Promise<UploadStatus>;
  getProcessing: (videoId: string) => Promise<ProcessingStatus>;
  cancelUpload: (uploadId: string) => Promise<void>;
  deleteVideo: (videoId: string) => Promise<void>;
}

export type UploadPhase = "idle" | "creating" | "starting" | "uploading" | "completing" | "processing" | "published" | "failed" | "canceled";

export interface UploadState {
  phase: UploadPhase;
  videoId: string | null;
  uploadId: string | null;
  progress: number;
  error: string | null;
}

type Action =
  | { type: "phase"; phase: UploadPhase }
  | { type: "ids"; videoId: string; uploadId?: string }
  | { type: "progress"; progress: number }
  | { type: "error"; message: string }
  | { type: "reset" };

const initialState: UploadState = { phase: "idle", videoId: null, uploadId: null, progress: 0, error: null };
const reducer = (state: UploadState, action: Action): UploadState => {
  if (action.type === "phase") return { ...state, phase: action.phase, error: null };
  if (action.type === "ids") return { ...state, videoId: action.videoId, uploadId: action.uploadId ?? state.uploadId };
  if (action.type === "progress") return { ...state, progress: action.progress };
  if (action.type === "error") return { ...state, phase: "failed", error: action.message };
  return initialState;
};

const isUnauthorized = (error: unknown): boolean => error instanceof ApiError && error.status === 401;
const isRetryable = (error: unknown): boolean =>
  !(error instanceof ApiError) || error.status >= 500;
const delay = (milliseconds: number, signal: AbortSignal): Promise<void> =>
  new Promise((resolve, reject) => {
    const timer = window.setTimeout(resolve, milliseconds);
    signal.addEventListener("abort", () => {
      window.clearTimeout(timer);
      reject(new DOMException("Subida cancelada.", "AbortError"));
    }, { once: true });
  });

const retry = async <T>(operation: () => Promise<T>, signal: AbortSignal): Promise<T> => {
  for (let attempt = 1; ; attempt += 1) {
    try {
      return await operation();
    } catch (error) {
      if (!isRetryable(error) || attempt >= MAX_UPLOAD_ATTEMPTS || signal.aborted) throw error;
      const jitter = Math.random() * UPLOAD_RETRY_BASE_MS;
      await delay(UPLOAD_RETRY_BASE_MS * 2 ** (attempt - 1) + jitter, signal);
    }
  }
};

const saveResume = (value: { uploadId: string; videoId: string; fileName: string; sizeBytes: number; contentType: string; chunkSize: number }): void =>
  sessionStorage.setItem(UPLOAD_SESSION_KEY, JSON.stringify(value));

export function useVideoUpload(clients: UploadClients, pollInterval = DEFAULT_UPLOAD_POLL_MS) {
  const [state, dispatch] = useReducer(reducer, initialState);
  const queryClient = useQueryClient();
  const controllerRef = useRef<AbortController | null>(null);
  const activeRef = useRef(false);

  const finish = useCallback((phase: UploadPhase, videoId: string | null) => {
    dispatch({ type: "phase", phase });
    sessionStorage.removeItem(UPLOAD_SESSION_KEY);
    void queryClient.invalidateQueries({ queryKey: ["quota"] });
    if (phase === "published" && videoId) {
      void queryClient.invalidateQueries({ queryKey: ["videos"] });
      void queryClient.invalidateQueries({ queryKey: ["video", videoId] });
    }
  }, [queryClient]);

  const cancel = useCallback(async (): Promise<void> => {
    controllerRef.current?.abort();
    const videoId = state.videoId;
    const uploadId = state.uploadId;
    if (state.phase === "completing" || state.phase === "processing" || state.phase === "published") return;
    if (uploadId) await Promise.resolve(clients.cancelUpload(uploadId)).catch(() => undefined);
    if (videoId) await Promise.resolve(clients.deleteVideo(videoId)).catch(() => undefined);
    finish("canceled", videoId);
  }, [clients, finish, state.phase, state.uploadId, state.videoId]);

  const run = useCallback(async (file: File, metadata: CreateVideoRequest, resume?: { uploadId: string; videoId: string; chunkSize: number }): Promise<void> => {
    if (activeRef.current) throw new Error("Ya hay una subida activa.");
    if (file.size === 0) throw new Error("El archivo no puede estar vacío.");
    activeRef.current = true;
    const controller = new AbortController();
    controllerRef.current = controller;
    let videoId = resume?.videoId ?? null;
    let uploadId = resume?.uploadId ?? null;
    let cleanupAllowed = true;
    const chunkSize = resume?.chunkSize ?? Number(import.meta.env.VITE_UPLOAD_CHUNK_SIZE ?? DEFAULT_UPLOAD_CHUNK_SIZE);
    const unload = (event: BeforeUnloadEvent): void => { event.preventDefault(); event.returnValue = ""; };
    window.addEventListener("beforeunload", unload);
    try {
      if (!videoId) {
        dispatch({ type: "phase", phase: "creating" });
        videoId = (await clients.createVideo(metadata)).id;
        dispatch({ type: "ids", videoId });
      }
      dispatch({ type: "phase", phase: "starting" });
      let started = resume ? await retry(() => clients.getUpload(uploadId!), controller.signal) : await retry(() => clients.startUpload(videoId!, {
        fileName: file.name, contentType: file.type, totalBytes: file.size, chunkSize,
      }), controller.signal);
      if (!resume) {
        uploadId = (started as StartUploadResponse).uploadId;
        saveResume({ uploadId, videoId, fileName: file.name, sizeBytes: file.size, contentType: file.type, chunkSize });
      }
      dispatch({ type: "ids", videoId, uploadId: uploadId ?? undefined });
      const hashes = await hashFileIncrementally(file, chunkSize);
      const receivedBytes = Number(started.receivedBytes ?? 0);
      const firstChunk = Math.floor(receivedBytes / chunkSize);
      dispatch({ type: "phase", phase: "uploading" });
      for (let chunkNumber = firstChunk; chunkNumber < hashes.chunkChecksums.length; chunkNumber += 1) {
        const start = chunkNumber * chunkSize;
        const chunk = file.slice(start, Math.min(start + chunkSize, file.size));
        await retry(() => clients.uploadChunk(uploadId!, chunkNumber, chunk, hashes.chunkChecksums[chunkNumber], `bytes ${start}-${start + chunk.size - 1}/${file.size}`, { signal: controller.signal }), controller.signal);
        dispatch({ type: "progress", progress: Math.round(((start + chunk.size) / file.size) * 100) });
      }
      dispatch({ type: "phase", phase: "completing" });
      cleanupAllowed = false;
      started = await retry(() => clients.completeUpload(uploadId!, { fileChecksum: hashes.fileChecksum }), controller.signal);
      dispatch({ type: "phase", phase: "processing" });
      let processing = await retry(() => clients.getProcessing(videoId!), controller.signal);
      while (processing.status === "processing") {
        await delay(pollInterval, controller.signal);
        processing = await retry(() => clients.getProcessing(videoId!), controller.signal);
      }
      finish(processing.status === "ready" ? "published" : "failed", videoId);
    } catch (error) {
      if (isUnauthorized(error)) throw error;
      let cleanupFailed = false;
      if (cleanupAllowed && uploadId) await Promise.resolve(clients.cancelUpload(uploadId)).catch(() => { cleanupFailed = true; });
      if (cleanupAllowed && videoId) await Promise.resolve(clients.deleteVideo(videoId)).catch(() => { cleanupFailed = true; });
      if (!controller.signal.aborted) {
        const message = error instanceof Error ? error.message : "No se pudo completar la subida.";
        dispatch({ type: "error", message: cleanupFailed ? `${message} No se pudo limpiar el registro ${videoId ?? ""}.` : message });
      }
      if (controller.signal.aborted) finish("canceled", videoId);
      else throw error;
    } finally {
      window.removeEventListener("beforeunload", unload);
      controllerRef.current = null;
      activeRef.current = false;
    }
  }, [clients, finish, pollInterval]);

  useEffect(() => () => { controllerRef.current?.abort(); }, []);
  return { state, start: run, cancel, hasResume: Boolean(sessionStorage.getItem(UPLOAD_SESSION_KEY)) };
}
