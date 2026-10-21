import { afterEach, describe, expect, it, vi } from "vitest";
import { setHttpToken } from "./httpClient";
import { streamingClient } from "./streamingClient";

describe("streamingClient.uploadChunk", () => {
  afterEach(() => {
    vi.restoreAllMocks();
    setHttpToken(null);
  });

  it("envía el Blob sin serializar, sus headers y el AbortSignal; acepta 204", async () => {
    const file = new Blob(["fragmento"]);
    const signal = new AbortController().signal;
    const fetchMock = vi.spyOn(globalThis, "fetch").mockResolvedValue(
      new Response(null, { status: 204 }),
    );
    setHttpToken("token");
    const onProgress = vi.fn();

    await streamingClient.uploadChunk(
      "upload-1",
      0,
      file,
      "a".repeat(64),
      "bytes 0-8/9",
      { signal, onProgress },
    );

    expect(fetchMock).toHaveBeenCalledOnce();
    const [url, init] = fetchMock.mock.calls[0];
    expect(url).toBe("http://localhost:8083/api/v1/uploads/upload-1/chunks/0");
    expect(init?.method).toBe("PUT");
    expect(init?.body).toBe(file);
    expect(init?.signal).toBe(signal);
    expect(new Headers(init?.headers).get("Content-Type")).toBe("application/octet-stream");
    expect(new Headers(init?.headers).get("X-Chunk-Checksum")).toBe("a".repeat(64));
    expect(new Headers(init?.headers).get("Content-Range")).toBe("bytes 0-8/9");
    expect(onProgress).toHaveBeenCalledWith(file.size);
  });
});
