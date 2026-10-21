import { describe, expect, it } from "vitest";
import { authStore } from "../auth/authStore";
import { mockUser } from "./data";

describe("DASH mock manifest", () => {
  it("serves BaseURL before the first AdaptationSet", async () => {
    authStore.setSession("mock-jwt", mockUser);
    const response = await fetch("http://localhost:8083/api/v1/videos/mock-video/manifest.mpd", {
      headers: { Authorization: "Bearer mock-jwt" },
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

describe("subidas mock", () => {
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
    expect((await status.json()).receivedBytes).toBe(3);
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
});
