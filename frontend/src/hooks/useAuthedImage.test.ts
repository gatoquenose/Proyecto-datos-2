import { renderHook, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import { authStore } from "../auth/authStore";
import { useAuthedImage } from "./useAuthedImage";

describe("useAuthedImage", () => {
  afterEach(() => {
    authStore.clear();
  });

  it("descarga con Authorization, crea un blob URL y lo libera al desmontar", async () => {
    authStore.setSession("test-token", {
      id: "550e8400-e29b-41d4-a716-446655440000",
      username: "ana.tec",
      email: "ana@example.test",
      quotaBytes: 100,
      usedBytes: 10,
    });
    const fetchMock = vi.fn().mockResolvedValue(new Response(new Blob(["image"]), { status: 200 }));
    vi.stubGlobal("fetch", fetchMock);
    const createObjectUrl = vi.fn().mockReturnValue("blob:test");
    const revokeObjectUrl = vi.fn();
    vi.stubGlobal("URL", { createObjectURL: createObjectUrl, revokeObjectURL: revokeObjectUrl });

    const { result, unmount } = renderHook(() => useAuthedImage("video-1", true));
    await waitFor(() => expect(result.current.url).toBe("blob:test"));

    expect(fetchMock).toHaveBeenCalledWith(
      "http://localhost:8083/api/v1/videos/video-1/thumbnail",
      expect.objectContaining({ headers: { Authorization: "Bearer test-token" } }),
    );
    unmount();
    expect(createObjectUrl).toHaveBeenCalled();
    expect(revokeObjectUrl).toHaveBeenCalledWith("blob:test");
  });

  it("muestra el marcador cuando la descarga falla", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(null, { status: 404 })));
    const { result } = renderHook(() => useAuthedImage("missing", true));
    await waitFor(() => expect(result.current.failed).toBe(true));
    expect(result.current.url).toBeNull();
  });
});
