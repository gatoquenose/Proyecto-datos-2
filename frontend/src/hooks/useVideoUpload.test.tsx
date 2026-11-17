import { act, renderHook } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { describe, expect, it, vi } from "vitest";
import { ApiError } from "../types/api";
import { DEFAULT_UPLOAD_CHUNK_SIZE, UPLOAD_SESSION_KEY, useVideoUpload } from "./useVideoUpload";

const wrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={new QueryClient({ defaultOptions: { queries: { retry: false } } })}>{children}</QueryClientProvider>;
const video = { id: "video-1" } as never;
const start = { uploadId: "upload-1", videoId: "video-1", reservedBytes: 100, receivedBytes: 0, status: "uploading" } as never;
const status = { uploadId: "upload-1", videoId: "video-1", status: "uploading", sizeBytes: 3, receivedChunks: [], totalChunks: 1, receivedBytes: 0 } as {
  uploadId: string; videoId: string; status: "uploading"; sizeBytes: number; receivedChunks: number[]; totalChunks: number; receivedBytes: number;
};
const metadata = { fileName: "video.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 * 1024 * 1024 };

const webCryptoSha256Hex = async (bytes: Uint8Array): Promise<string> => {
  const digest = await crypto.subtle.digest("SHA-256", new Uint8Array(bytes).buffer);
  return Array.from(new Uint8Array(digest), (byte) => byte.toString(16).padStart(2, "0")).join("");
};

const withFakeTimers = async (run: () => Promise<void>): Promise<void> => {
  // jsdom lee Blob con FileReader (sin arrayBuffer); 250 * 2 ** (intento - 1) + 0.5 * 250 da 375 y 625 ms.
  vi.useFakeTimers({ toFake: ["setTimeout", "clearTimeout"] });
  try {
    await run();
  } finally {
    vi.useRealTimers();
  }
};

const createFailedCompleteRetry = async (pollInterval = 0) => {
  const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
  const clients = {
    createVideo: vi.fn().mockResolvedValue(video),
    startUpload: vi.fn().mockResolvedValue(start),
    uploadChunk: vi.fn().mockResolvedValue(undefined),
    getUpload: vi.fn(),
    completeUpload: vi.fn().mockRejectedValueOnce(invalid),
    getProcessing: vi.fn(),
    cancelUpload: vi.fn().mockResolvedValue(undefined),
    deleteVideo: vi.fn().mockResolvedValue(undefined),
  };
  const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  const invalidateQueries = vi.spyOn(queryClient, "invalidateQueries");
  const localWrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>;
  const hook = renderHook(() => useVideoUpload(clients, pollInterval), { wrapper: localWrapper });
  const file = new File(["abc"], "video.mp4", { type: "video/mp4" });

  await act(async () => {
    await expect(hook.result.current.start(file, metadata as never)).rejects.toThrow("Checksum inválido");
  });

  return { ...hook, clients, file, invalidateQueries };
};

describe("useVideoUpload", () => {
  it.each(["", "no-numérico", "0", "-1", "1.5", "Infinity"])(
    "usa el tamaño de fragmento por defecto con VITE_UPLOAD_CHUNK_SIZE=%s",
    async (configuredChunkSize) => {
      vi.stubEnv("VITE_UPLOAD_CHUNK_SIZE", configuredChunkSize);
      const clients = {
        createVideo: vi.fn().mockResolvedValue(video),
        startUpload: vi.fn().mockResolvedValue(start),
        uploadChunk: vi.fn().mockResolvedValue(undefined),
        getUpload: vi.fn(),
        completeUpload: vi.fn().mockResolvedValue(status),
        getProcessing: vi.fn().mockResolvedValue({ status: "ready" }),
        cancelUpload: vi.fn(),
        deleteVideo: vi.fn(),
      };
      const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });

      await act(async () => {
        await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never);
      });

      expect(clients.startUpload.mock.calls[0][1].chunkSize).toBe(DEFAULT_UPLOAD_CHUNK_SIZE);
    },
  );

  it("acepta VITE_UPLOAD_CHUNK_SIZE=1 sin imponer un mínimo práctico", async () => {
    vi.stubEnv("VITE_UPLOAD_CHUNK_SIZE", "1");
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video),
      startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined),
      getUpload: vi.fn(),
      completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }),
      cancelUpload: vi.fn(),
      deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });

    await act(async () => {
      await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never);
    });

    expect(clients.startUpload.mock.calls[0][1].chunkSize).toBe(1);
    expect(clients.uploadChunk).toHaveBeenCalledTimes(3);
  });

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
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toBeNull();
  });

  it("reintenta un ApiError 5xx", async () => {
    const transient = new ApiError(503, { code: "SERVICE_UNAVAILABLE", message: "down", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValueOnce(transient).mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => { await result.current.start(new File(["a"], "video.mp4", { type: "video/mp4" }), metadata as never); });
    expect(clients.uploadChunk).toHaveBeenCalledTimes(2);
  });

  it("no reintenta un ApiError 4xx", async () => {
    const permanent = new ApiError(400, { code: "VALIDATION_ERROR", message: "bad", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockRejectedValue(permanent),
      uploadChunk: vi.fn(), getUpload: vi.fn(), completeUpload: vi.fn(),
      getProcessing: vi.fn(), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => {
      await expect(result.current.start(new File(["a"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("bad");
    });
    expect(clients.startUpload).toHaveBeenCalledTimes(1);
    expect(clients.uploadChunk).not.toHaveBeenCalled();
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

  it("no vuelve a leer ni rehashear el archivo durante un retry de fragmento", async () => {
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValueOnce(new TypeError("Failed to fetch")).mockResolvedValue(undefined),
      getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const file = new File(["abc"], "video.mp4", { type: "video/mp4" });
    const slice = vi.spyOn(file, "slice");
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });

    await act(async () => { await result.current.start(file, metadata as never); });

    expect(clients.uploadChunk).toHaveBeenCalledTimes(2);
    expect(slice).toHaveBeenCalledTimes(2);
    expect(clients.completeUpload).toHaveBeenCalledWith("upload-1", {
      fileChecksum: await webCryptoSha256Hex(new TextEncoder().encode("abc")),
    });
  });

  it("no reintenta un AbortError", async () => {
    await withFakeTimers(async () => {
      const canceled = new DOMException("Aborted", "AbortError");
      const clients = {
        createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
        uploadChunk: vi.fn().mockRejectedValue(canceled), getUpload: vi.fn(), completeUpload: vi.fn(),
        getProcessing: vi.fn(), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
      };
      const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
      let running: Promise<{ value: void | undefined; error: unknown }> | undefined;
      await act(async () => {
        running = result.current.start(new File(["a"], "video.mp4", { type: "video/mp4" }), metadata as never)
          .then((value) => ({ value, error: undefined as unknown }))
          .catch((error: unknown) => ({ value: undefined, error }));
        await vi.advanceTimersByTimeAsync(0);
        await vi.runAllTimersAsync();
        await running;
      });
      if (!running) throw new Error("La subida no inició.");
      expect((await running).error).toBeInstanceOf(DOMException);
      expect(((await running).error as DOMException).name).toBe("AbortError");
      expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
    });
  });

  it("no reintenta un Error genérico", async () => {
    await withFakeTimers(async () => {
      const failure = new Error("unexpected");
      const clients = {
        createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
        uploadChunk: vi.fn().mockRejectedValue(failure), getUpload: vi.fn(), completeUpload: vi.fn(),
        getProcessing: vi.fn(), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
      };
      const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
      let running: Promise<{ value: void | undefined; error: unknown }> | undefined;
      await act(async () => {
        running = result.current.start(new File(["a"], "video.mp4", { type: "video/mp4" }), metadata as never)
          .then((value) => ({ value, error: undefined as unknown }))
          .catch((error: unknown) => ({ value: undefined, error }));
        await vi.advanceTimersByTimeAsync(0);
        await vi.runAllTimersAsync();
        await running;
      });
      if (!running) throw new Error("La subida no inició.");
      expect((await running).error).toBe(failure);
      expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
    });
  });

  it("espera 375 ms y luego 625 ms entre retries con jitter fijo", async () => {
    await withFakeTimers(async () => {
      vi.spyOn(Math, "random").mockReturnValue(0.5);
      const clients = {
        createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
        uploadChunk: vi.fn().mockRejectedValueOnce(new ApiError(503, { code: "SERVICE_UNAVAILABLE", message: "down", details: null, traceId: "x" }))
          .mockRejectedValueOnce(new ApiError(503, { code: "SERVICE_UNAVAILABLE", message: "down", details: null, traceId: "x" }))
          .mockResolvedValue(undefined),
        getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
        getProcessing: vi.fn().mockResolvedValue({ status: "ready" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
      };
      const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
      const file = new File(["a"], "video.mp4", { type: "video/mp4" });
      const flushMicrotasks = async (): Promise<void> => {
        for (let index = 0; index < 20; index += 1) await Promise.resolve();
      };
      let running: Promise<void> | undefined;

      await act(async () => {
        running = result.current.start(file, metadata as never);
        await vi.advanceTimersByTimeAsync(0);
        await flushMicrotasks();
      });
      expect(clients.uploadChunk).toHaveBeenCalledTimes(1);

      await act(async () => { await vi.advanceTimersByTimeAsync(374); });
      expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
      await act(async () => { await vi.advanceTimersByTimeAsync(1); await flushMicrotasks(); });
      expect(clients.uploadChunk).toHaveBeenCalledTimes(2);

      await act(async () => { await vi.advanceTimersByTimeAsync(624); });
      expect(clients.uploadChunk).toHaveBeenCalledTimes(2);
      await act(async () => { await vi.advanceTimersByTimeAsync(1); await running; });
      expect(clients.uploadChunk).toHaveBeenCalledTimes(3);
      expect(result.current.state.phase).toBe("published");
    });
  });

  it.each([403, 413])("reconoce QUOTA_EXCEEDED por código con HTTP %i", async (httpStatus) => {
    const quota = new ApiError(httpStatus, {
      code: "QUOTA_EXCEEDED",
      message: "Respuesta genérica del servidor",
      details: null,
      traceId: "x",
    });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video),
      startUpload: vi.fn().mockRejectedValue(quota),
      uploadChunk: vi.fn(),
      getUpload: vi.fn(),
      completeUpload: vi.fn(),
      getProcessing: vi.fn(),
      cancelUpload: vi.fn(),
      deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });

    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never))
        .rejects.toThrow("Respuesta genérica del servidor");
    });

    expect(result.current.state.error).toBe("No tienes suficiente cuota disponible.");
    expect(clients.deleteVideo).toHaveBeenCalledWith("video-1");
    expect(clients.startUpload).toHaveBeenCalledTimes(1);
  });

  it("agota los reintentos de red, pasa a failed y limpia el huérfano", async () => {
    vi.spyOn(Math, "random").mockReturnValue(0);
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValue(new TypeError("Failed to fetch")),
      getUpload: vi.fn(), completeUpload: vi.fn(), getProcessing: vi.fn(),
      cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const invalidateQueries = vi.spyOn(queryClient, "invalidateQueries");
    const localWrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>;
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper: localWrapper });
    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow("Failed to fetch");
    });
    expect(clients.uploadChunk).toHaveBeenCalledTimes(3);
    expect(clients.cancelUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.deleteVideo).toHaveBeenCalledWith("video-1");
    expect(result.current.state.phase).toBe("failed");
    expect(invalidateQueries).toHaveBeenCalledWith({ queryKey: ["quota"] });
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
    expect(clients.completeUpload.mock.calls[0][1].fileChecksum)
      .toBe(await webCryptoSha256Hex(new TextEncoder().encode("abc")));
    expect(result.current.state.phase).toBe("published");
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toBeNull();
  });

  it("tras recargar un 400, relee bytes enviados y completa con el hash de una pasada", async () => {
    vi.stubEnv("VITE_UPLOAD_CHUNK_SIZE", "2");
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video),
      startUpload: vi.fn().mockResolvedValue({ uploadId: "upload-1", videoId: "video-1", reservedBytes: 100, receivedBytes: 0, status: "uploading", chunkSizeBytes: 2 }),
      uploadChunk: vi.fn().mockResolvedValue(undefined),
      getUpload: vi.fn().mockResolvedValue({ uploadId: "upload-1", videoId: "video-1", status: "uploading", sizeBytes: 4, receivedChunks: [0, 1], totalChunks: 2, receivedBytes: 4, chunkSizeBytes: 2 }),
      completeUpload: vi.fn().mockRejectedValueOnce(invalid).mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "ready" }),
      cancelUpload: vi.fn().mockResolvedValue(undefined),
      deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const file = new File(["abcd"], "video.mp4", { type: "video/mp4" });
    const firstRender = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => {
      await expect(firstRender.result.current.start(file, metadata as never)).rejects.toThrow("Checksum inválido");
    });
    const savedResume = JSON.parse(sessionStorage.getItem(UPLOAD_SESSION_KEY)!);

    const reloaded = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => {
      await reloaded.result.current.start(file, metadata as never, savedResume);
    });

    expect(clients.getUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.uploadChunk).toHaveBeenCalledTimes(2);
    const fullFileChecksum = await webCryptoSha256Hex(new TextEncoder().encode("abcd"));
    expect(clients.completeUpload).toHaveBeenNthCalledWith(1, "upload-1", { fileChecksum: fullFileChecksum });
    expect(clients.completeUpload).toHaveBeenNthCalledWith(2, "upload-1", { fileChecksum: fullFileChecksum });
    expect(reloaded.result.current.state.phase).toBe("published");
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toBeNull();
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

  it("retira beforeunload al fallar el envío de un fragmento", async () => {
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Fragmento inválido", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video),
      startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockRejectedValue(invalid),
      getUpload: vi.fn(),
      completeUpload: vi.fn(),
      getProcessing: vi.fn(),
      cancelUpload: vi.fn().mockResolvedValue(undefined),
      deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const add = vi.spyOn(window, "addEventListener");
    const remove = vi.spyOn(window, "removeEventListener");
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });

    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never))
        .rejects.toThrow("Fragmento inválido");
    });

    const unloadHandler = add.mock.calls.find(([event]) => event === "beforeunload")?.[1];
    expect(unloadHandler).toBeDefined();
    expect(remove).toHaveBeenCalledWith("beforeunload", unloadHandler);
  });

  it("cancela a media subida con AbortSignal y limpia el registro", async () => {
    let resolveChunk: (() => void) | undefined;
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockImplementation(() => new Promise<void>((resolve) => { resolveChunk = resolve; })),
      getUpload: vi.fn(), completeUpload: vi.fn(), getProcessing: vi.fn(),
      cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const invalidateQueries = vi.spyOn(queryClient, "invalidateQueries");
    const localWrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>;
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper: localWrapper });
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
    expect(invalidateQueries).toHaveBeenCalledWith({ queryKey: ["quota"] });
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

  it("conserva estado y registro si completeUpload vuelve a responder 400", async () => {
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video),
      startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined),
      getUpload: vi.fn().mockResolvedValue({ ...status, receivedBytes: 3 }),
      completeUpload: vi.fn().mockRejectedValue(invalid),
      getProcessing: vi.fn(),
      cancelUpload: vi.fn().mockResolvedValue(undefined),
      deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    const file = new File(["abc"], "video.mp4", { type: "video/mp4" });

    await act(async () => {
      await expect(result.current.start(file, metadata as never)).rejects.toThrow("Checksum inválido");
    });
    await act(async () => {
      await expect(result.current.retryFailed(file, metadata as never)).rejects.toThrow("Checksum inválido");
    });

    expect(result.current.state.phase).toBe("failed");
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toContain("upload-1");
    expect(clients.startUpload).toHaveBeenCalledTimes(1);
    expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
    expect(clients.completeUpload).toHaveBeenCalledTimes(2);
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
  });

  it("descarta tras el 400 de completeUpload y limpia la sesión", async () => {
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockRejectedValue(invalid),
      getProcessing: vi.fn(), cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never)).rejects.toThrow();
    });
    await act(async () => { await result.current.discardFailed(); });

    expect(clients.cancelUpload).toHaveBeenCalledWith("upload-1");
    expect(clients.deleteVideo).toHaveBeenCalledWith("video-1");
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toBeNull();
  });

  it("reintenta completeUpload tras un 400 conservando la subida y el checksum", async () => {
    const invalid = new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x" });
    let releaseProcessing: ((status: { status: "processing" }) => void) | undefined;
    let notifyProcessingStarted: (() => void) | undefined;
    const processingStarted = new Promise<void>((resolve) => { notifyProcessingStarted = resolve; });
    const processingResponse = new Promise<{ status: "processing" }>((resolve) => { releaseProcessing = resolve; });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn().mockResolvedValue({ ...status, receivedBytes: 3 }), completeUpload: vi.fn().mockRejectedValueOnce(invalid).mockResolvedValue(status),
      getProcessing: vi.fn()
        .mockImplementationOnce(() => { notifyProcessingStarted?.(); return processingResponse; })
        .mockResolvedValueOnce({ status: "ready" }),
      cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    const file = new File(["abc"], "video.mp4", { type: "video/mp4" });
    await act(async () => { await expect(result.current.start(file, metadata as never)).rejects.toThrow("Checksum inválido"); });
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toContain("upload-1");
    const remoteUpload = await clients.getUpload("upload-1");
    expect(remoteUpload.status).toBe("uploading");
    await expect(clients.startUpload.mock.results[0].value).resolves.toHaveProperty("reservedBytes", 100);
    let retrying: Promise<void> | undefined;
    await act(async () => {
      retrying = result.current.retryFailed(file, metadata as never);
      await processingStarted;
    });
    expect(clients.getProcessing).toHaveBeenCalledTimes(1);
    expect(result.current.state.phase).toBe("processing");
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    await act(async () => {
      releaseProcessing?.({ status: "processing" });
      await retrying;
    });
    const fileChecksum = await webCryptoSha256Hex(new TextEncoder().encode("abc"));
    expect(clients.completeUpload).toHaveBeenNthCalledWith(1, "upload-1", { fileChecksum });
    expect(clients.completeUpload).toHaveBeenNthCalledWith(2, "upload-1", { fileChecksum });
    expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
    expect(clients.startUpload).toHaveBeenCalledTimes(1);
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.getProcessing).toHaveBeenCalledTimes(2);
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toBeNull();
    expect(result.current.state.phase).toBe("published");
  });

  it("aborta el sondeo del reintento al desmontar el hook sin finalizar estado", async () => {
    const { result, unmount, clients, file, invalidateQueries } = await createFailedCompleteRetry(25);
    invalidateQueries.mockClear();
    let notifyPollStarted: (() => void) | undefined;
    const pollStarted = new Promise<void>((resolve) => { notifyPollStarted = resolve; });
    clients.getProcessing
      .mockImplementationOnce(() => {
        notifyPollStarted?.();
        return Promise.resolve({ status: "processing" });
      })
      .mockResolvedValueOnce({ status: "ready" });
    let retrying: Promise<void> | undefined;

    await withFakeTimers(async () => {
      await act(async () => {
        retrying = result.current.retryFailed(file, metadata as never).catch(() => undefined);
        await pollStarted;
      });
      unmount();
      await act(async () => { await vi.advanceTimersByTimeAsync(25); });
      await retrying;
    });

    expect(clients.getProcessing).toHaveBeenCalledTimes(1);
    expect(invalidateQueries).not.toHaveBeenCalled();
  });

  it("cancel() aborta el sondeo activo del reintento", async () => {
    const { result, clients, file, invalidateQueries } = await createFailedCompleteRetry(25);
    invalidateQueries.mockClear();
    let notifyPollStarted: (() => void) | undefined;
    const pollStarted = new Promise<void>((resolve) => { notifyPollStarted = resolve; });
    clients.getProcessing
      .mockImplementationOnce(() => {
        notifyPollStarted?.();
        return Promise.resolve({ status: "processing" });
      })
      .mockResolvedValueOnce({ status: "ready" });
    let retrying: Promise<void> | undefined;

    await withFakeTimers(async () => {
      await act(async () => {
        retrying = result.current.retryFailed(file, metadata as never).catch(() => undefined);
        await pollStarted;
      });
      await act(async () => { await result.current.cancel(); });
      await act(async () => { await vi.advanceTimersByTimeAsync(25); });
      await retrying;
    });

    expect(clients.getProcessing).toHaveBeenCalledTimes(1);
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
    expect(invalidateQueries).not.toHaveBeenCalled();
  });

  it("no inicia un segundo completeUpload si ya hay un reintento activo", async () => {
    const { result, clients, file } = await createFailedCompleteRetry();
    let releaseComplete: ((value: typeof status) => void) | undefined;
    let notifyCompleteStarted: (() => void) | undefined;
    const completeStarted = new Promise<void>((resolve) => { notifyCompleteStarted = resolve; });
    const completeResponse = new Promise<typeof status>((resolve) => { releaseComplete = resolve; });
    clients.completeUpload.mockClear();
    clients.completeUpload.mockImplementation(() => {
      notifyCompleteStarted?.();
      return completeResponse;
    });
    clients.getProcessing.mockResolvedValue({ status: "ready" });
    let firstRetry: Promise<void> | undefined;
    let secondRetry: Promise<void> | undefined;

    await act(async () => {
      firstRetry = result.current.retryFailed(file, metadata as never);
      await completeStarted;
      secondRetry = result.current.retryFailed(file, metadata as never);
    });
    releaseComplete?.(status);
    await act(async () => { await Promise.allSettled([firstRetry, secondRetry]); });

    expect(clients.completeUpload).toHaveBeenCalledTimes(1);
    await expect(secondRetry).resolves.toBeUndefined();
  });

  it("registra y retira beforeunload durante el reintento exitoso", async () => {
    const { result, clients, file } = await createFailedCompleteRetry();
    const add = vi.spyOn(window, "addEventListener");
    const remove = vi.spyOn(window, "removeEventListener");
    add.mockClear();
    remove.mockClear();
    clients.completeUpload.mockResolvedValue(status);
    clients.getProcessing.mockResolvedValue({ status: "ready" });

    await act(async () => { await result.current.retryFailed(file, metadata as never); });

    const unloadHandler = add.mock.calls.find(([event]) => event === "beforeunload")?.[1];
    expect(unloadHandler).toBeDefined();
    expect(remove).toHaveBeenCalledWith("beforeunload", unloadHandler);
  });

  it("registra y retira beforeunload si completeUpload vuelve a fallar", async () => {
    const { result, clients, file } = await createFailedCompleteRetry();
    const add = vi.spyOn(window, "addEventListener");
    const remove = vi.spyOn(window, "removeEventListener");
    add.mockClear();
    remove.mockClear();
    clients.completeUpload.mockRejectedValue(new ApiError(400, {
      code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "x",
    }));

    await act(async () => {
      await expect(result.current.retryFailed(file, metadata as never)).rejects.toThrow("Checksum inválido");
    });

    const unloadHandler = add.mock.calls.find(([event]) => event === "beforeunload")?.[1];
    expect(unloadHandler).toBeDefined();
    expect(remove).toHaveBeenCalledWith("beforeunload", unloadHandler);
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
    expect(sessionStorage.getItem(UPLOAD_SESSION_KEY)).toBeNull();
  });

  it("sondea processing hasta ready e invalida cuota, lista y video al publicar", async () => {
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValueOnce({ status: "processing" }).mockResolvedValueOnce({ status: "ready" }),
      cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const queryClient = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const invalidate = vi.spyOn(queryClient, "invalidateQueries");
    const localWrapper = ({ children }: { children: React.ReactNode }) => <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>;
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper: localWrapper });
    await act(async () => { await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never); });
    expect(clients.getProcessing).toHaveBeenCalledTimes(2);
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["quota"] });
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["videos"] });
    expect(invalidate).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
  });

  it("detiene el sondeo cuando el procesamiento termina en failed", async () => {
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockResolvedValue({ status: "failed" }), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    await act(async () => { await result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never); });

    expect(clients.getProcessing).toHaveBeenCalledTimes(1);
    expect(result.current.state.phase).toBe("failed");
  });

  it("detiene el sondeo ante 401", async () => {
    const error = new ApiError(401, { code: "TOKEN_EXPIRED", message: "Sesión expirada", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockRejectedValue(error), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });

    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never))
        .rejects.toThrow("Sesión expirada");
    });

    expect(clients.getProcessing).toHaveBeenCalledTimes(1);
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
  });

  it("detiene el sondeo ante 403", async () => {
    const error = new ApiError(403, { code: "FORBIDDEN", message: "No disponible", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockRejectedValue(error), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });

    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never))
        .rejects.toThrow("No disponible");
    });

    expect(clients.getProcessing).toHaveBeenCalledTimes(1);
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
  });

  it("detiene el sondeo ante 404", async () => {
    const error = new ApiError(404, { code: "NOT_FOUND", message: "No disponible", details: null, traceId: "x" });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn().mockRejectedValue(error), cancelUpload: vi.fn(), deleteVideo: vi.fn(),
    };
    const { result } = renderHook(() => useVideoUpload(clients), { wrapper });

    await act(async () => {
      await expect(result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never))
        .rejects.toThrow("No disponible");
    });

    expect(clients.getProcessing).toHaveBeenCalledTimes(1);
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
  });

  it("no permite cancelar tras completeUpload exitoso", async () => {
    let releaseProcessing: ((status: { status: "processing" }) => void) | undefined;
    let notifyProcessingStarted: (() => void) | undefined;
    const processingStarted = new Promise<void>((resolve) => { notifyProcessingStarted = resolve; });
    const processingResponse = new Promise<{ status: "processing" }>((resolve) => { releaseProcessing = resolve; });
    const clients = {
      createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
      uploadChunk: vi.fn().mockResolvedValue(undefined), getUpload: vi.fn(), completeUpload: vi.fn().mockResolvedValue(status),
      getProcessing: vi.fn()
        .mockImplementationOnce(() => {
          notifyProcessingStarted?.();
          return processingResponse;
        })
        .mockResolvedValueOnce({ status: "ready" }),
      cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
    };
    const { result } = renderHook(() => useVideoUpload(clients, 0), { wrapper });
    let running: Promise<void> | undefined;

    await act(async () => {
      running = result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never);
      await processingStarted;
    });
    expect(clients.completeUpload).toHaveBeenCalledTimes(1);
    expect(result.current.state.phase).toBe("processing");

    await act(async () => { await result.current.cancel(); });
    expect(clients.cancelUpload).not.toHaveBeenCalled();
    expect(clients.deleteVideo).not.toHaveBeenCalled();
    expect(result.current.state.phase).toBe("processing");

    await act(async () => {
      releaseProcessing?.({ status: "processing" });
      await running;
    });
    expect(result.current.state.phase).toBe("published");
  });

  it("interrumpe la espera de retry al cancelar", async () => {
    await withFakeTimers(async () => {
      vi.spyOn(Math, "random").mockReturnValue(0);
      const transient = new ApiError(503, { code: "SERVICE_UNAVAILABLE", message: "down", details: null, traceId: "x" });
      const clients = {
        createVideo: vi.fn().mockResolvedValue(video), startUpload: vi.fn().mockResolvedValue(start),
        uploadChunk: vi.fn().mockRejectedValue(transient), getUpload: vi.fn(), completeUpload: vi.fn(),
        getProcessing: vi.fn(), cancelUpload: vi.fn().mockResolvedValue(undefined), deleteVideo: vi.fn().mockResolvedValue(undefined),
      };
      const { result } = renderHook(() => useVideoUpload(clients), { wrapper });
      let running: Promise<void> | undefined;
      const flushMicrotasks = async (): Promise<void> => {
        for (let index = 0; index < 20; index += 1) await Promise.resolve();
      };
      await act(async () => {
        running = result.current.start(new File(["abc"], "video.mp4", { type: "video/mp4" }), metadata as never);
        await vi.advanceTimersByTimeAsync(0);
        await flushMicrotasks();
      });
      expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
      await act(async () => { await result.current.cancel(); await running; });
      expect(clients.uploadChunk).toHaveBeenCalledTimes(1);
      expect(result.current.state.phase).toBe("canceled");
    });
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
