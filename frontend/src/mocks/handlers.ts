import { http, HttpResponse } from "msw";
import { mockUser, mockVideos } from "./data";

const error = (status: number, code: string, message: string, details: Record<string, unknown> | null = null) =>
  HttpResponse.json({ code, message, details, traceId: "mock-trace-001" }, { status });

const authorized = (request: Request): boolean => request.headers.get("Authorization") === "Bearer mock-jwt";

export const handlers = [
  http.post("http://localhost:8081/api/v1/users", async ({ request }) => {
    const body = await request.json() as { username?: string; email?: string };
    if (body.username === "existente") return error(409, "USERNAME_EXISTS", "El usuario ya existe.");
    return HttpResponse.json({ id: crypto.randomUUID(), username: body.username ?? "nuevo" }, { status: 201 });
  }),
  http.post("http://localhost:8081/api/v1/auth/login", async ({ request }) => {
    const body = await request.json() as { username?: string; password?: string };
    if (!body.username || !body.password) return error(401, "INVALID_TOKEN", "Credenciales inválidas.");
    return HttpResponse.json({ accessToken: "mock-jwt", tokenType: "Bearer", expiresIn: 3600, user: mockUser });
  }),
  http.get("http://localhost:8081/api/v1/users/me", ({ request }) =>
    authorized(request) ? HttpResponse.json(mockUser) : error(401, "MISSING_TOKEN", "Token requerido.")),
  http.get("http://localhost:8081/api/v1/users/me/quota", ({ request }) =>
    authorized(request) ? HttpResponse.json({ quotaBytes: mockUser.quotaBytes, usedBytes: mockUser.usedBytes, remainingBytes: mockUser.quotaBytes - mockUser.usedBytes }) : error(401, "MISSING_TOKEN", "Token requerido.")),
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
];
