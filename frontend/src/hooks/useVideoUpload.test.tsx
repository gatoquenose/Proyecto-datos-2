import { act, renderHook } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { describe, expect, it, vi } from "vitest";
import { ApiError } from "../types/api";
import { UPLOAD_SESSION_KEY, useVideoUpload } from "./useVideoUpload";

const wrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={new QueryClient({ defaultOptions: { queries: { retry: false } } })}>{children}</QueryClientProvider>;
const video = { id: "video-1" } as never;
const start = { uploadId: "upload-1", videoId: "video-1", reservedBytes: 100, receivedBytes: 0, status: "uploading" } as never;
const status = { uploadId: "upload-1", videoId: "video-1", status: "uploading", sizeBytes: 3, receivedChunks: [], totalChunks: 1, receivedBytes: 0 } as {
  uploadId: string; videoId: string; status: "uploading"; sizeBytes: number; receivedChunks: number[]; totalChunks: number; receivedBytes: number;
};
const metadata = { fileName: "video.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 * 1024 * 1024 };

describe("useVideoUpload", () => {
  it("ejecuta metadata, inicio, fragmentos, completar y procesamiento", async () => {
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => { await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never); });
    expect(clients.createVideo).toHaveBeenCalled();
    expect(clients.startUpload).toHaveBeenCalled();
    expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
    expect(clients.completeUpload).toHaveBeenCalled();
    expect(result.current.state.phase).toBe("published");
  });

  it("reintenta 5xx pero no 4xx", async () => {
    const transient = new ApiError(503, { code: "SERVICE_UNAVAILABLE", message: "down", details: null, traceId: "x" });
    const permanent = new ApiError(400, { code: "VALIDATION_ERROR", message: "bad", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValueOnce(transient).mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => { await result.current.start(new File(["a"], "video.mp4", { type: "video/mp4" }), metadata as never); });
    expect(clients.uploadChunk).toHaveBeenCalledTimes(2);
    clients.startUpload.mockRejectedValueOnce(permanent);
    await act(async () => { await expect(result.current.start(new File(["a"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("bad"); });
    expect(clients.startUpload).toHaveBeenCalledTimes(2);
  });

  it("reintenta un error de red TypeError y publica", async () => {
    vi.spyOn(Math, "random").mockReturnValue(0);
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValueOnce(new TypeError("Failed to fetch")).mockResolvedValue(undefined),
      getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => { await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never); });
    expect(clients.uploadChunk).toHaveBeenCalledTimes(2);
    expect(result.current.state.phase).toBe("published");
  });

  it("agota los reintentos de red, pasa a failed y limpia el huérfano", async () => {
    vi.spyOn(Math, "random").mockReturnValue(0);
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValue(new TypeError("Failed to fetch")),
      getUpload: vi.fn(), completeUpload: vi.fn(), getProcessing: vi.fn(),
      cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("Failed to fetch");
    });
    expect(clients.uploadChunk).toHaveBeenCalledTimes(3);
    expect(clients.cancelUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.deleteVideo).toHaveBeenCalledWith("video-1");
    expect(result.current.state.phase).toBe("failed");
  });

  it("no limpia ante 401 y conserva la reanudación", async () => {
    const unauthorized = new ApiError(401, { code: "TOKEN_EXPIRED", message: "expired", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValue(unauthorized), getUpload: vi.fn(), completeUpload: vi.fn(),
      getProcessing: vi.fn(), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => { await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("expired"); });
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toContain("upload-1");
  });

  it("limpia el huérfano si la cuota se rechaza", async () => {
    const quota = new ApiError(413, { code: "QUOTA_EXCEEDED", message: "quota", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockRejectedValue(quota),
      uploadChunk: vi.fn(), getUpload: vi.fn(), completeUpload: vi.fn(), getProcessing: vi.fn(),
      cancelUpload: vi.fn(), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => { await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("quota"); });
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).toHaveBeenCalledWith("video-1");
    expect(result.current.state.videoId).toBe("video-1");
  });

  it("reanuda desde receivedBytes sin volver a subir los fragmentos recibidos", async () => {
    sessionStorage.setItem(UPLOAD_SESSION_KEY, JSON.stringify({ uploadId: "upload-1", videoId: "video-1", fileName: "video.mp4", sizeBytes: 3, contentType: "video/mp4", chunkSize: 2 }));
    const clients = {
      createVideo: vi.fn(), startUpload: vi.fn(), uploadChunk: vi.fn().mockResolvedValue(undefined),
      getUpload: vi.fn().mockResolvedValue({ ...status, receivedBytes: 2, chunkSizeBytes: 2 }),
      completeUpload: vi.fn().mockResolvedValue(status), getProcessing: vi.fn().mockResolvedValue({ status: "ready" }),
      cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => { await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never, JSON.parse(sessionStorage.getItem(UPLOAD_SESSION_KEY)!)); });
    expect(clients.createVideo).not.toHaveBeenCalled();
    expect(clients.getUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
    expect(clients.uploadChunk.mock.calls[0][1]).toBe(1);
    expect(clients.completeUpload.mock.calls[0][1].fileChecksum).toBe("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    expect(result.current.state.phase).toBe("published");
  });

  it("registra y retira beforeunload al finalizar", async () => {
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const add = vi.spyOn(window, "addEventListener");
    const remove = vi.spyOn(window, "removeEventListener");
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => { await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never); });
    expect(add).toHaveBeenCalledWith("beforeunload", expect.any(Function));
    expect(remove).toHaveBeenCalledWith("beforeunload", expect.any(Function));
  });

  it("cancela a media subida con AbortSignal y limpia el registro", async () => {
    let resolveChunk: (() => void) | undefined;
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockImplementation(() => new Promise<void>((resolve) => { resolveChunk = resolve; })),
      getUpload: vi.fn(), completeUpload: vi.fn(), getProcessing: vi.fn(),
      cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    let running: Promise<void> | undefined;
    await act(async () => {
      running = result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never);
      await Promise.resolve();
      await Promise.resolve();
    });
    await act(async () => { await result.current.cancel(); });
    resolveChunk?.();
    await act(async () => { await running; });
    expect(clients.uploadChunk.mock.calls[0][5]?.signal).toBeDefined();
    expect(clients.cancelUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.deleteVideo).toHaveBeenCalledWith("video-1");
    expect(result.current.state.phase).toBe("canceled");
  });

  it("deja failed y conserva la reanudación si el checksum final es rechazado", async () => {
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockRejectedValue(invalid),
      getProcessing: vi.fn(), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => { await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("Checksum inválido"); });
    expect(result.current.state.phase).toBe("failed");
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toContain("upload-1");
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
  });

  it("ofrece reintentar tras checksum inválido y conserva el video", async () => {
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockRejectedValueOnce(invalid).mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    const file = new File(["abc"], "video.mp4", { type: "video/mp4" });
    await act(async () => { await expect(result.current.start(file, metadata as never)).rejects.toThrow("Checksum inválido"); });
    await act(async () => { await result.current.retryFailed(file, metadata as never); });
    expect(clients.cancelUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.startUpload).toHaveBeenCalledTimes(2);
    expect(result.current.state.phase).toBe("published");
  });

  it("ofrece descartar tras checksum inválido e invalida cuota", async () => {
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockRejectedValue(invalid),
      getProcessing: vi.fn(), cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const invalidate = vi.spyOn(queryClient, "invalidateQueries");
    const localWrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>;
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper: localWrapper });
    await act(async () => { await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow(); });
    await act(async () => { await result.current.discardFailed(); });
    expect(clients.cancelUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.deleteVideo).toHaveBeenCalledWith("video-1");
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["quota"] });
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["videos"] });
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
  });

  it("invalida cuota, lista y video al publicar", async () => {
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const invalidate = vi.spyOn(queryClient, "invalidateQueries");
    const localWrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>;
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper: localWrapper });
    await act(async () => { await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never); });
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["quota"] });
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["videos"] });
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
  });

  it("interrumpe la espera de retry al cancelar", async () => {
    const transient = new ApiError(503, { code: "SERVICE_UNAVAILABLE", message: "down", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValue(transient), getUpload: vi.fn(), completeUpload: vi.fn(),
      getProcessing: vi.fn(), cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    let running: Promise<void> | undefined;
    await act(async () => {
      running = result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never);
      await Promise.resolve();
      await Promise.resolve();
    });
    await act(async () => { await result.current.cancel(); await running; });
    expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
    expect(result.current.state.phase).toBe("canceled");
  });

  it("informa si no puede limpiar el registro huérfano", async () => {
    const quota = new ApiError(413, { code: "QUOTA_EXCEEDED", message: "Cuota insuficiente", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockRejectedValue(quota),
      uploadChunk: vi.fn(), getUpload: vi.fn(), completeUpload: vi.fn(), getProcessing: vi.fn(),
      cancelUpload: vi.fn(), deleteVideo: vi.fn().mockRejectedValue(new Error("metadata down")),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => { await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("Cuota insuficiente"); });
    expect(result.current.state.videoId).toBe("video-1");
    expect(result.current.state.error).toContain("No se pudo limpiar el registro video-1");
  });
});
