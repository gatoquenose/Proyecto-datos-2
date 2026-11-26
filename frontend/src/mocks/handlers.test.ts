import { readFile } from "node:fs/promises";
import { resolve } from "node:path";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { authStore } from "../auth/authStore";
import { getMockQuota, mockPassword, mockUser, mockVideos } from "./data";
import { configureMockUploadControls, mockUploadControls } from "./handlers";

const STREAMING_BASE_URL = "http://localhost:8083/api/v1";
const METADATA_BASE_URL = "http://localhost:8082/api/v1";
const DASH_SEGMENT_NAMES = [
  "init-0.m4s", "init-1.m4s", "init-2.m4s",
  "seg-0-00001.m4s", "seg-0-00002.m4s", "seg-0-00003.m4s",
  "seg-1-00001.m4s", "seg-1-00002.m4s", "seg-1-00003.m4s",
  "seg-2-00001.m4s", "seg-2-00002.m4s", "seg-2-00003.m4s",
] as const;
const mockAuthorization = async (): Promise<HeadersInit> => {
  const response = await fetch("http://localhost:8081/api/v1/auth/login", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ username: mockUser.username, password: mockPassword }),
  });
  const session = await response.json() as { accessToken: string; tokenType: string };
  return { Authorization: `${session.tokenType} ${session.accessToken}` };
};
const assetByPath = new Map(
  DASH_SEGMENT_NAMES.map((name) => [`/src/mocks/fixtures/dash/${name}`, name]),
);
// These fixture paths assume Vitest runs with frontend as process.cwd().
const fixturePath = (name: (typeof DASH_SEGMENT_NAMES)[number]): string =>
  resolve(process.cwd(), "src", "mocks", "fixtures", "dash", name);
let savedVideos: typeof mockVideos = [];

beforeEach(() => {
  savedVideos = mockVideos.map((video) => ({ ...video, tags: [...video.tags] }));
});

afterEach(() => {
  mockVideos.splice(0, mockVideos.length, ...savedVideos);
  vi.unstubAllGlobals();
});

const installPassthroughAssetFetchAdapter = (): void => {
  const originalFetch = globalThis.fetch;
  vi.stubGlobal("fetch", (input: RequestInfo | URL, init?: RequestInit): Promise<Response> => {
    const request = input instanceof Request ? input : new Request(input, init);
    if (request.headers.get("accept") === "msw/passthrough") {
      const name = assetByPath.get(new URL(request.url).pathname);
      if (name) {
        return readFile(fixturePath(name))
          .then((bytes) => new Response(bytes));
      }
    }
    return originalFetch(input, init);
  });
};

describe("DASH mock manifest", () => {
  it("serves BaseURL before the first AdaptationSet", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const response = await fetch("http://localhost:8083/api/v1/videos/mock-video/manifest.mpd", {
      headers: await mockAuthorization(),
    });
    const manifest = new DOMParser().parseFromString(await response.text(), "application/xml");
    const parserError = manifest.querySelector("parsererror");
    const baseUrl = manifest.getElementsByTagNameNS("urn:mpeg:dash:schema:mpd:2011", "BaseURL")[0];
    const firstAdaptationSet = manifest.getElementsByTagNameNS("urn:mpeg:dash:schema:mpd:2011", "AdaptationSet")[0];

    expect(response.ok).toBe(true);
    expect(parserError).toBeNull();
    expect(baseUrl?.textContent).toBe("dash/");
    expect(baseUrl?.compareDocumentPosition(firstAdaptationSet) & Node.DOCUMENT_POSITION_FOLLOWING).toBeTruthy();
  });
});

describe("DASH mock segments", () => {
  let authorization: HeadersInit;

  beforeEach(async () => {
    authorization = await mockAuthorization();
    installPassthroughAssetFetchAdapter();
  });

  it.each(DASH_SEGMENT_NAMES)("serves %s bytes from the fixture with the representation MIME type", async (name) => {
    const representation = name.startsWith("init-") ? name.charAt(5) : name.charAt(4);
    const contentType = representation === "2" ? "audio/mp4" : "video/mp4";
    const response = await fetch(`${STREAMING_BASE_URL}/videos/mock-video/dash/${name}`, {
      headers: authorization,
    });
    const actual = new Uint8Array(await response.arrayBuffer());
    const expected = new Uint8Array(await readFile(fixturePath(name)));

    // This verifies the mock handler and does not demonstrate playback in a browser.
    expect(response.status).toBe(200);
    expect(response.headers.get("Content-Type")).toBe(contentType);
    expect(actual).toEqual(expected);
  });

  it("requires authorization for segments", async () => {
    const response = await fetch(`${STREAMING_BASE_URL}/videos/mock-video/dash/init-0.m4s`);

    expect(response.status).toBe(401);
    expect(await response.json()).toMatchObject({
      code: "MISSING_TOKEN",
      traceId: "mock-trace-001",
    });
  });

  it("returns a common 404 error for unknown segment names", async () => {
    const response = await fetch(`${STREAMING_BASE_URL}/videos/mock-video/dash/unknown.m4s`, {
      headers: authorization,
    });

    expect(response.status).toBe(404);
    expect(await response.json()).toMatchObject({
      code: "NOT_FOUND",
      message: "Segmento DASH no encontrado.",
      details: null,
      traceId: "mock-trace-001",
    });
  });
});

describe("edición de metadata mock", () => {
  const videoId = mockVideos.find((video) => video.ownerId === mockUser.id)?.id;
  if (!videoId) throw new Error("Se requiere un video propio para probar la edición.");

  const sendUpdate = (body: unknown, authorization?: HeadersInit, id = videoId): Promise<Response> =>
    fetch(`${METADATA_BASE_URL}/videos/${id}`, {
      method: "PATCH",
      headers: { ...authorization, "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });

  const expectCommonError = async (response: Response, status: number, code: string): Promise<void> => {
    expect(response.status).toBe(status);
    const body = await response.json() as { code: string; message: string; details: unknown; traceId: string };
    expect(body).toMatchObject({
      code,
      message: expect.any(String),
      traceId: "mock-trace-001",
    });
    expect(body).toHaveProperty("details");
  };

  it("actualiza metadata visible en detalle y lista", async () => {
    const authorization = await mockAuthorization();
    const response = await sendUpdate({ title: "Título editado en mocks" }, authorization);
    const detailResponse = await fetch(`${METADATA_BASE_URL}/videos/${videoId}`, { headers: authorization });
    const listResponse = await fetch(
      `${METADATA_BASE_URL}/videos?owner=me&q=${encodeURIComponent("Título editado en mocks")}&pageSize=50`,
      { headers: authorization },
    );

    expect(response.status).toBe(200);
    expect((await detailResponse.json()).title).toBe("Título editado en mocks");
    expect((await listResponse.json()).items).toEqual(
      expect.arrayContaining([expect.objectContaining({ id: videoId, title: "Título editado en mocks" })]),
    );
  });

  it("preserva los campos que no se editaron", async () => {
    const authorization = await mockAuthorization();
    const original = mockVideos.find((video) => video.id === videoId);
    if (!original) throw new Error("El video de prueba dejó de existir.");
    const expected = {
      ownerId: original.ownerId,
      ownerUsername: original.ownerUsername,
      durationSeconds: original.durationSeconds,
      status: original.status,
      sizeBytes: original.sizeBytes,
      visibility: original.visibility,
      tags: [...original.tags],
    };

    await sendUpdate({ title: "Solo cambia el título" }, authorization);
    const response = await fetch(`${METADATA_BASE_URL}/videos/${videoId}`, { headers: authorization });

    expect(await response.json()).toMatchObject({ title: "Solo cambia el título", ...expected });
  });

  it("acepta los límites máximos definidos por el contrato", async () => {
    const authorization = await mockAuthorization();
    const body = {
      title: "t".repeat(200),
      description: "d".repeat(5000),
      author: "a".repeat(200),
      topic: "t".repeat(100),
      tags: Array.from({ length: 30 }, () => "g".repeat(50)),
      uploadedAt: "2026-11-17T06:00:00Z",
    };

    const response = await sendUpdate(body, authorization);

    expect(response.status).toBe(200);
    expect(await response.json()).toMatchObject(body);
  });

  it("rechaza un body sin propiedades", async () => {
    await expectCommonError(await sendUpdate({}, await mockAuthorization()), 400, "VALIDATION_ERROR");
  });

  it("rechaza propiedades no permitidas", async () => {
    await expectCommonError(
      await sendUpdate({ title: "Título válido", status: "ready" }, await mockAuthorization()),
      400,
      "VALIDATION_ERROR",
    );
  });

  it.each([
    ["title vacío", { title: "" }],
    ["title de más de 200 caracteres", { title: "t".repeat(201) }],
    ["description de más de 5000 caracteres", { description: "d".repeat(5001) }],
    ["author de más de 200 caracteres", { author: "a".repeat(201) }],
    ["topic de más de 100 caracteres", { topic: "t".repeat(101) }],
    ["más de 30 tags", { tags: Array.from({ length: 31 }, () => "tag") }],
    ["tag de más de 50 caracteres", { tags: ["g".repeat(51)] }],
    ["uploadedAt que no es date-time", { uploadedAt: "2026-13-40T25:61:61Z" }],
  ])("rechaza %s", async (_caseName, body) => {
    await expectCommonError(await sendUpdate(body, await mockAuthorization()), 400, "VALIDATION_ERROR");
  });

  it("requiere autenticación", async () => {
    await expectCommonError(await sendUpdate({ title: "Sin credencial" }), 401, "MISSING_TOKEN");
  });

  it("devuelve 404 para un video inexistente", async () => {
    await expectCommonError(
      await sendUpdate({ title: "Título" }, await mockAuthorization(), "video-inexistente"),
      404,
      "NOT_FOUND",
    );
  });

  it("devuelve 403 para un video de otro usuario", async () => {
    const foreignVideo = mockVideos.find((video) => video.ownerId !== mockUser.id);
    if (!foreignVideo) throw new Error("Se requiere un video ajeno para probar permisos.");

    await expectCommonError(
      await sendUpdate({ title: "Sin permiso" }, await mockAuthorization(), foreignVideo.id),
      403,
      "FORBIDDEN",
    );
  });
});

describe("subidas mock", () => {
  const startUpload = async (videoId = "mock-video"): Promise<{ uploadId: string }> => {
    const response = await fetch(`${STREAMING_BASE_URL}/videos/${videoId}/upload`, {
      method: "POST", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileName: "a.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 }),
    });
    return await response.json() as { uploadId: string };
  };

  const chunkRequest = (uploadId: string): Promise<Response> => fetch(`http://localhost:8083/api/v1/uploads/${uploadId}/chunks/0`, {
    method: "PUT",
    headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/octet-stream", "X-Chunk-Checksum": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "Content-Range": "bytes 0-2/3" },
    body: new Uint8Array([97, 98, 99]),
  });

  it("crea con duración cero, publica con 12 segundos y conserva la duración precargada", async () => {
    const authorization = await mockAuthorization();
    const preloaded = mockVideos.find((video) => video.status === "ready");
    if (!preloaded) throw new Error("Se requiere un video precargado listo para esta prueba.");
    const originalDurations = mockVideos.map(({ id, durationSeconds }) => ({ id, durationSeconds }));
    const createdResponse = await fetch(`${METADATA_BASE_URL}/videos`, {
      method: "POST",
      headers: { ...authorization, "Content-Type": "application/json" },
      body: JSON.stringify({ title: "Video DASH nuevo", visibility: "private", sizeBytes: 3 }),
    });
    const created = await createdResponse.json() as { id: string; durationSeconds: number };
    const upload = await startUpload(created.id);
    const chunkResponse = await chunkRequest(upload.uploadId);
    const checksum = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const completeResponse = await fetch(`${STREAMING_BASE_URL}/uploads/${upload.uploadId}/complete`, {
      method: "POST",
      headers: { ...authorization, "Content-Type": "application/json" },
      body: JSON.stringify({ fileChecksum: checksum }),
    });
    await fetch(`${STREAMING_BASE_URL}/videos/${created.id}/processing`, { headers: authorization });
    const processingResponse = await fetch(`${STREAMING_BASE_URL}/videos/${created.id}/processing`, { headers: authorization });
    const published = await fetch(`${METADATA_BASE_URL}/videos/${created.id}`, { headers: authorization });

    expect(createdResponse.status).toBe(201);
    expect(originalDurations).toHaveLength(30);
    expect(created.durationSeconds).toBe(0);
    expect(chunkResponse.status).toBe(204);
    expect(completeResponse.status).toBe(202);
    expect((await processingResponse.json()).status).toBe("ready");
    expect((await published.json()).durationSeconds).toBe(12);
    expect(mockVideos.filter((video) => originalDurations.some(({ id }) => id === video.id))
      .map(({ id, durationSeconds }) => ({ id, durationSeconds }))).toEqual(originalDurations);
  });

  it("valida checksum, orden y receivedBytes de fragmentos", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const start = await fetch("http://localhost:8083/api/v1/videos/mock-video/upload", {
      method: "POST",
      headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileName: "a.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 }),
    });
    const upload = await start.json() as { uploadId: string };
    const bytes = new Uint8Array([97, 98, 99]);
    const wrong = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/chunks/0`, {
      method: "PUT", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/octet-stream", "X-Chunk-Checksum": "0".repeat(64), "Content-Range": "bytes 0-2/3" }, body: bytes,
    });
    expect(wrong.status).toBe(400);
    const accepted = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/chunks/0`, {
      method: "PUT", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/octet-stream", "X-Chunk-Checksum": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "Content-Range": "bytes 0-2/3" }, body: bytes,
    });
    const status = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}`, { headers: { Authorization: "Bearer mock-jwt" } });
    expect(accepted.status).toBe(204);
    const completed = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/complete`, {
      method: "POST", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileChecksum: "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" }),
    });
    expect(completed.status).toBe(202);
    expect((await status.json()).receivedBytes).toBe(3);
  });

  it("valida el checksum final y permite cancelar una subida inválida", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const start = await fetch("http://localhost:8083/api/v1/videos/mock-video/upload", {
      method: "POST", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileName: "a.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 }),
    });
    const upload = await start.json() as { uploadId: string };
    const invalid = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/complete`, {
      method: "POST", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileChecksum: "0".repeat(64) }),
    });
    expect(invalid.status).toBe(400);
    const canceled = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}`, {
      method: "DELETE",       headers: { Authorization: "Bearer mock-jwt" },
    });
    expect(canceled.status).toBe(204);
    const missing = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}`, { headers: { Authorization: "Bearer mock-jwt" } });
    expect(missing.status).toBe(404);
  });

  it("inyecta 5xx una vez y 400 sin consumir el fragmento", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const start = await fetch("http://localhost:8083/api/v1/videos/mock-video/upload", {
      method: "POST",       headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileName: "a.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 }),
    });
    const upload = await start.json() as { uploadId: string };
    const headers = { Authorization: "Bearer mock-jwt", "Content-Type": "application/octet-stream", "X-Chunk-Checksum": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "Content-Range": "bytes 0-2/3" };
    mockUploadControls.failChunkWith5xxOnce = 0;
    expect((await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/chunks/0`, { method: "PUT", headers, body: new Uint8Array([97, 98, 99]) })).status).toBe(503);
    expect((await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/chunks/0`, { method: "PUT", headers, body: new Uint8Array([97, 98, 99]) })).status).toBe(204);
    mockUploadControls.failChunkWith400 = true;
    expect((await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/chunks/1`, { method: "PUT", headers, body: new Uint8Array([97, 98, 99]) })).status).toBe(400);
    mockUploadControls.failChunkWith400 = false;
  });

  it("inyecta un error de red en el fragmento", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const start = await fetch("http://localhost:8083/api/v1/videos/mock-video/upload", {
      method: "POST", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileName: "a.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 }),
    });
    const upload = await start.json() as { uploadId: string };
    mockUploadControls.failNetwork = true;
    const request = fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/chunks/0`, {
      method: "PUT",
      headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/octet-stream" },
      body: new Uint8Array([97, 98, 99]),
    });
    await expect(request).rejects.toThrow(TypeError);
    mockUploadControls.failNetwork = false;
  });

  it("rechaza la reserva si el multiplicador excede la cuota", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const response = await fetch("http://localhost:8083/api/v1/videos/mock-video/upload", {
      method: "POST",
      headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileName: "large.mp4", contentType: "video/mp4", totalBytes: mockUser.quotaBytes, chunkSize: 8 }),
    });
    expect(response.status).toBe(413);
    expect((await response.json()).code).toBe("QUOTA_EXCEEDED");
  });

  it.each(["5xx-once:0", "5xx-always:0"])("configura fallo 5xx mediante %s", async (failure) => {
    vi.stubEnv("VITE_MOCK_UPLOAD_FAILURE", failure);
    configureMockUploadControls();
    authStore.setSession("mock-jwt", mockUser);
    const upload = await startUpload();
    expect((await chunkRequest(upload.uploadId)).status).toBe(503);
    expect((await chunkRequest(upload.uploadId)).status).toBe(failure.startsWith("5xx-always") ? 503 : 204);
  });

  it.each(["network-once:0", "network-always:0"])("configura fallo de red mediante %s", async (failure) => {
    vi.stubEnv("VITE_MOCK_UPLOAD_FAILURE", failure);
    configureMockUploadControls();
    authStore.setSession("mock-jwt", mockUser);
    const upload = await startUpload();
    await expect(chunkRequest(upload.uploadId)).rejects.toThrow(TypeError);
    if (failure.startsWith("network-once")) expect((await chunkRequest(upload.uploadId)).status).toBe(204);
  });

  it("configura fallo 400 en un fragmento", async () => {
    vi.stubEnv("VITE_MOCK_UPLOAD_FAILURE", "400:0");
    configureMockUploadControls();
    authStore.setSession("mock-jwt", mockUser);
    const upload = await startUpload();
    expect((await chunkRequest(upload.uploadId)).status).toBe(400);
  });

  it("configura checksum final inválido", async () => {
    vi.stubEnv("VITE_MOCK_UPLOAD_FAILURE", "checksum-final");
    configureMockUploadControls();
    authStore.setSession("mock-jwt", mockUser);
    const upload = await startUpload();
    const response = await fetch(`http://localhost:8083/api/v1/uploads/${upload.uploadId}/complete`, {
      method: "POST", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileChecksum: "0".repeat(64) }),
    });
    expect(response.status).toBe(400);
  });

  it("usa una cuota casi llena coherente en quota y reserva", async () => {
    vi.stubEnv("VITE_MOCK_QUOTA_REMAINING_BYTES", "4");
    authStore.setSession("mock-jwt", mockUser);
    const quota = await fetch("http://localhost:8081/api/v1/users/me/quota", { headers: { Authorization: "Bearer mock-jwt" } });
    expect(await quota.json()).toEqual(getMockQuota());
    const response = await fetch("http://localhost:8083/api/v1/videos/mock-video/upload", {
      method: "POST", headers: { Authorization: "Bearer mock-jwt", "Content-Type": "application/json" },
      body: JSON.stringify({ fileName: "a.mp4", contentType: "video/mp4", totalBytes: 3, chunkSize: 8 }),
    });
    expect(response.status).toBe(413);
  });

  it("ignora una configuración de fallo inválida y avisa en desarrollo", () => {
    const warning = vi.spyOn(console, "warn").mockImplementation(() => undefined);
    vi.stubEnv("VITE_MOCK_UPLOAD_FAILURE", "invalid");
    configureMockUploadControls();
    expect(warning).toHaveBeenCalledWith("VITE_MOCK_UPLOAD_FAILURE inválida: invalid");
  });

  it("devuelve una miniatura PNG 1x1 válida", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const response = await fetch("http://localhost:8083/api/v1/videos/mock-video/thumbnail", {
      headers: { Authorization: "Bearer mock-jwt" },
    });
    expect(response.status).toBe(200);
    const buffer = new Uint8Array(await response.arrayBuffer());

    // 1. Firma PNG (8 bytes)
    const pngSignature = [137, 80, 78, 71, 13, 10, 26, 10];
    expect(Array.from(buffer.slice(0, 8))).toEqual(pngSignature);

    // Helper CRC32
    const makeCrcTable = () => {
      let c;
      const crcTable = [];
      for (let n = 0; n < 256; n++) {
        c = n;
        for (let k = 0; k < 8; k++) {
          c = ((c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1));
        }
        crcTable[n] = c;
      }
      return crcTable;
    };
    const crcTable = makeCrcTable();
    const computeCrc = (buf: Uint8Array) => {
      let crc = 0xffffffff;
      for (let i = 0; i < buf.length; i++) {
        crc = (crc >>> 8) ^ crcTable[(crc ^ buf[i]) & 0xff];
      }
      return (crc ^ 0xffffffff) >>> 0;
    };

    // Parse chunks
    let offset = 8;
    const chunks: { length: number; type: string; data: Uint8Array; crc: number }[] = [];
    while (offset < buffer.length) {
      // Chunk format: length(4), type(4), data(length), crc(4)
      // offset points to start of length
      const length = new DataView(buffer.buffer, buffer.byteOffset + offset, 4).getUint32(0);
      const typeBytes = buffer.slice(offset + 4, offset + 8);
      const type = String.fromCharCode(...typeBytes);
      const data = buffer.slice(offset + 8, offset + 8 + length);
      const crc = new DataView(buffer.buffer, buffer.byteOffset + offset + 8 + length, 4).getUint32(0);


      // Validar CRC del chunk (tipo + datos)
      const typeAndData = new Uint8Array(4 + length);
      typeAndData.set(typeBytes, 0);
      typeAndData.set(data, 4);

      const calculatedCrc = computeCrc(typeAndData);
      expect(calculatedCrc).toBe(crc);

      chunks.push({ length, type, data, crc });
      offset += 12 + length;
    }

    // Verificar chunk IHDR
    const ihdr = chunks.find((c) => c.type === "IHDR");
    expect(ihdr).toBeDefined();
    if (ihdr) {
      const dv = new DataView(ihdr.data.buffer, ihdr.data.byteOffset, ihdr.data.byteLength);
      expect(dv.getUint32(0)).toBe(1); // ancho
      expect(dv.getUint32(4)).toBe(1); // alto
    }

    // Verificar chunk IEND (últimos 12 bytes)
    const lastChunk = chunks[chunks.length - 1];
    expect(lastChunk.type).toBe("IEND");
    const expectedIendTrailer = [0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130];
    expect(Array.from(buffer.slice(buffer.length - 12))).toEqual(expectedIendTrailer);
  });
});
