import { http, HttpResponse } from "msw";
import { mockPassword, mockUser, mockVideos } from "./data";
import manifest from "./fixtures/dash/manifest.mpd?raw";
import { hashFileIncrementally, sha256Hex } from "../utils/checksum";

export const mockUploadControls = {
  failChunkWith5xxOnce: null as number | null,
  failNetwork: false,
  failChunkWith400: false,
  delayMs: Number(import.meta.env.VITE_MOCK_UPLOAD_DELAY_MS ?? 120),
};
const mockUploads = new Map<string, {
  uploadId: string; videoId: string; sizeBytes: number; chunkSizeBytes: number;
  receivedChunks: number[]; receivedBytes: number; chunks: Map<number, Uint8Array>;
  fileName: string; contentType: string; status: "uploading" | "queued" | "processing" | "published" | "failed"; processingChecks: number;
}>();

const error = (status: number, code: string, message: string, details: Record<string, unknown> | null = null) =>
  HttpResponse.json({ code, message, details, traceId: "mock-trace-001" }, { status });

const authorized = (request: Request): boolean => ["Bearer mock-jwt", "******"].includes(request.headers.get("Authorization") ?? "");

export const handlers = [
  http.post("http://localhost:8081/api/v1/users", async ({ request }) => {
    const body = await request.json() as { username?: string; email?: string };
    if (body.username === "existente") return error(409, "USERNAME_EXISTS", "El usuario ya existe.");
    return HttpResponse.json({ id: crypto.randomUUID(), username: body.username ?? "nuevo" }, { status: 201 });
  }),
  http.post("http://localhost:8081/api/v1/auth/login", async ({ request }) => {
    const body = await request.json() as { username?: string; password?: string };
    if (body.username !== mockUser.username || body.password !== mockPassword) return error(401, "INVALID_CREDENTIALS", "Usuario o contraseña incorrectos.");
    return HttpResponse.json({ accessToken: "mock-jwt", tokenType: "Bearer", expiresIn: 3600, user: mockUser });
  }),
  http.get("http://localhost:8081/api/v1/users/me", ({ request }) =>
    authorized(request) ? HttpResponse.json(mockUser) : error(401, "MISSING_TOKEN", "Token requerido.")),
  http.get("http://localhost:8081/api/v1/users/me/quota", ({ request }) =>
    authorized(request) ? HttpResponse.json({ quotaBytes: mockUser.quotaBytes, usedBytes: mockUser.usedBytes, remainingBytes: mockUser.quotaBytes - mockUser.usedBytes }) : error(401, "MISSING_TOKEN", "Token requerido.")),
  http.patch("http://localhost:8081/api/v1/users/:id", async ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const body = await request.json() as { email?: string };
    if (body.email === "existente@example.test") return error(409, "EMAIL_EXISTS", "Ese correo ya existe.");
    return HttpResponse.json({ ...mockUser, id: String(params.id), email: body.email ?? mockUser.email });
  }),
  http.delete("http://localhost:8081/api/v1/users/:id", ({ request }) =>
    authorized(request) ? new HttpResponse(null, { status: 204 }) : error(401, "MISSING_TOKEN", "Token requerido.")),
  http.get("http://localhost:8082/api/v1/videos", ({ request }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const url = new URL(request.url);
    const q = (url.searchParams.get("q") ?? "").toLowerCase();
    const owner = url.searchParams.get("owner");
    const sortBy = url.searchParams.get("sortBy") ?? "uploadedAt";
    const order = url.searchParams.get("order") ?? "desc";
    const cursor = Number(url.searchParams.get("cursor") ?? "0");
    const pageSize = Math.min(Number(url.searchParams.get("pageSize") ?? "12"), 50);
    const filtered = mockVideos.filter((video) => (!q || `${video.title} ${video.author} ${video.topic}`.toLowerCase().includes(q)) && (owner !== "me" || video.ownerId === mockUser.id));
    filtered.sort((a, b) => {
      const left = sortBy === "title" ? a.title : sortBy === "author" ? a.ownerUsername : a.uploadedAt;
      const right = sortBy === "title" ? b.title : sortBy === "author" ? b.ownerUsername : b.uploadedAt;
      return order === "asc" ? left.localeCompare(right) : right.localeCompare(left);
    });
    const items = filtered.slice(cursor, cursor + pageSize);
    return HttpResponse.json({ items, nextCursor: cursor + pageSize < filtered.length ? String(cursor + pageSize) : null });
  }),
  http.post("http://localhost:8082/api/v1/videos", async ({ request }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const body = await request.json() as Record<string, unknown>;
    const now = new Date().toISOString();
    const video = {
      id: crypto.randomUUID(), ownerId: mockUser.id, ownerUsername: mockUser.username,
      title: String(body.title ?? ""), description: String(body.description ?? ""),
      author: String(body.author ?? ""), topic: String(body.topic ?? ""), tags: Array.isArray(body.tags) ? body.tags as string[] : [],
      durationSeconds: 0, uploadedAt: now, visibility: body.visibility === "public" ? "public" as const : "private" as const,
      sizeBytes: Number(body.sizeBytes ?? 0), status: "processing" as const, createdAt: now, updatedAt: now,
    };
    mockVideos.unshift(video);
    return HttpResponse.json(video, { status: 201 });
  }),
  http.get("http://localhost:8082/api/v1/videos/:id", ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const video = mockVideos.find((item) => item.id === String(params.id));
    return video ? HttpResponse.json(video) : error(404, "NOT_FOUND", "Video no encontrado.");
  }),
  http.patch("http://localhost:8082/api/v1/videos/:id/visibility", async ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const video = mockVideos.find((item) => item.id === String(params.id));
    if (!video || video.ownerId !== mockUser.id) return error(403, "FORBIDDEN", "Sin permisos.");
    const body = await request.json() as { visibility?: "public" | "private" };
    video.visibility = body.visibility ?? video.visibility;
    return HttpResponse.json(video);
  }),
  http.delete("http://localhost:8082/api/v1/videos/:id", ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const index = mockVideos.findIndex((item) => item.id === String(params.id));
    if (index < 0 || mockVideos[index].ownerId !== mockUser.id) return error(403, "FORBIDDEN", "Sin permisos.");
    mockVideos.splice(index, 1);
    return new HttpResponse(null, { status: 204 });
  }),
  http.get("http://localhost:8083/api/v1/videos/:videoId/processing", ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const video = mockVideos.find((item) => item.id === String(params.videoId));
    const upload = [...mockUploads.values()].find((item) => item.videoId === String(params.videoId));
    if (upload?.status === "processing") {
      upload.processingChecks += 1;
      if (upload.processingChecks >= 2) {
        upload.status = "published";
        if (video) video.status = "ready";
      }
    }
    const status = upload?.status === "processing" ? "processing" : upload?.status === "published" ? "ready" : video?.status;
    return video ? HttpResponse.json({ videoId: video.id, status, durationSeconds: video.durationSeconds, error: status === "failed" ? "Error mock" : null }) : error(404, "NOT_FOUND", "Video no encontrado.");
  }),
  http.post("http://localhost:8083/api/v1/videos/:videoId/upload", async ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const body = await request.json() as { fileName: string; contentType: string; totalBytes: number; chunkSize: number };
    const remainingBytes = mockUser.quotaBytes - mockUser.usedBytes;
    const reservationMultiplier = 1.5;
    if (body.totalBytes * reservationMultiplier > remainingBytes) return error(413, "QUOTA_EXCEEDED", "Cuota insuficiente.", { requiredBytes: body.totalBytes * reservationMultiplier });
    const uploadId = crypto.randomUUID();
    mockUploads.set(uploadId, {
      uploadId, videoId: String(params.videoId), sizeBytes: body.totalBytes, chunkSizeBytes: body.chunkSize,
      receivedChunks: [], receivedBytes: 0, chunks: new Map(), fileName: body.fileName, contentType: body.contentType, status: "uploading", processingChecks: 0,
    });
    return HttpResponse.json({ uploadId, videoId: String(params.videoId), reservedBytes: body.totalBytes * reservationMultiplier, receivedBytes: 0, status: "uploading" }, { status: 201 });
  }),
  http.put("http://localhost:8083/api/v1/uploads/:uploadId/chunks/:chunkNumber", async ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const upload = mockUploads.get(String(params.uploadId));
    if (!upload) return error(404, "NOT_FOUND", "Subida no encontrada.");
    const chunkNumber = Number(params.chunkNumber);
    if (chunkNumber !== upload.receivedChunks.length) return error(409, "CONFLICT", "Orden de fragmentos inválido.");
    if (mockUploadControls.failNetwork) throw new Error("Fallo de red simulado.");
    if (mockUploadControls.failChunkWith5xxOnce === chunkNumber) {
      mockUploadControls.failChunkWith5xxOnce = null;
      return error(503, "SERVICE_UNAVAILABLE", "Fallo 5xx simulado.");
    }
    if (mockUploadControls.failChunkWith400) return error(400, "VALIDATION_ERROR", "Fallo 4xx simulado.");
    const range = request.headers.get("Content-Range") ?? "";
    const expectedStart = chunkNumber * upload.chunkSizeBytes;
    const bytes = new Uint8Array(await request.arrayBuffer());
    const expectedEnd = expectedStart + bytes.byteLength - 1;
    if (range !== `bytes ${expectedStart}-${expectedEnd}/${upload.sizeBytes}`) return error(400, "VALIDATION_ERROR", "Content-Range inválido.");
    if (request.headers.get("X-Chunk-Checksum") !== await sha256Hex(bytes)) return error(400, "VALIDATION_ERROR", "Checksum inválido.");
    if (mockUploadControls.delayMs > 0) await new Promise((resolve) => setTimeout(resolve, mockUploadControls.delayMs));
    upload.chunks.set(chunkNumber, bytes);
    upload.receivedChunks.push(chunkNumber);
    upload.receivedBytes += bytes.byteLength;
    return new HttpResponse(null, { status: 204 });
  }),
  http.get("http://localhost:8083/api/v1/uploads/:uploadId", ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const upload = mockUploads.get(String(params.uploadId));
    if (!upload) return error(404, "NOT_FOUND", "Subida no encontrada.");
    return HttpResponse.json({ uploadId: upload.uploadId, videoId: upload.videoId, status: upload.status, sizeBytes: upload.sizeBytes, chunkSizeBytes: upload.chunkSizeBytes, receivedChunks: upload.receivedChunks, totalChunks: Math.ceil(upload.sizeBytes / upload.chunkSizeBytes), receivedBytes: upload.receivedBytes });
  }),
  http.post("http://localhost:8083/api/v1/uploads/:uploadId/complete", async ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    const upload = mockUploads.get(String(params.uploadId));
    if (!upload) return error(404, "NOT_FOUND", "Subida no encontrada.");
    const body = await request.json() as { fileChecksum: string };
    const bytes = new Uint8Array(upload.sizeBytes);
    upload.chunks.forEach((chunk, number) => bytes.set(chunk, number * upload.chunkSizeBytes));
    const expected = await hashFileIncrementally(new Blob([bytes]), upload.chunkSizeBytes);
    if (body.fileChecksum !== expected.fileChecksum) return error(400, "VALIDATION_ERROR", "Checksum de archivo inválido.");
    upload.status = "processing";
    return HttpResponse.json({ uploadId: upload.uploadId, videoId: upload.videoId, status: "processing", sizeBytes: upload.sizeBytes, chunkSizeBytes: upload.chunkSizeBytes, receivedChunks: upload.receivedChunks, totalChunks: upload.receivedChunks.length, receivedBytes: upload.receivedBytes }, { status: 202 });
  }),
  http.delete("http://localhost:8083/api/v1/uploads/:uploadId", ({ request, params }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    mockUploads.delete(String(params.uploadId));
    return new HttpResponse(null, { status: 204 });
  }),
  http.get("http://localhost:8083/api/v1/videos/:videoId/manifest.mpd", ({ request }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    return new HttpResponse(manifest.replace("<Period", "<BaseURL>dash/</BaseURL><Period"), {
      status: 200,
      headers: { "Content-Type": "application/dash+xml" },
    });
  }),
  http.get("http://localhost:8083/api/v1/videos/:videoId/thumbnail", ({ request }) => {
    if (!authorized(request)) return error(401, "MISSING_TOKEN", "Token requerido.");
    return new HttpResponse(new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10]), {
      status: 200,
      headers: { "Content-Type": "image/png" },
    });
  }),
];
