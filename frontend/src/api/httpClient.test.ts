import { afterEach, describe, expect, it, vi } from "vitest";
import { ApiError } from "../types/api";
import { request, setHttpToken, setUnauthorizedHandler } from "./httpClient";

afterEach(() => { vi.restoreAllMocks(); setHttpToken(null); setUnauthorizedHandler(null); });

describe("httpClient", () => {
  it("parsea errores comunes", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify({ code: "NOT_FOUND", message: "No", details: {}, traceId: "t1" }), { status: 404 })));
    await expect(request("/missing")).rejects.toMatchObject({ status: 404, body: { code: "NOT_FOUND", traceId: "t1" } } as Partial<ApiError>);
  });
  it("notifica 401 con la ruta de retorno", async () => {
    const handler = vi.fn();
    setUnauthorizedHandler(handler);
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify({ code: "TOKEN_EXPIRED", message: "Expiró", details: null, traceId: "t2" }), { status: 401 })));
    await expect(request("/private", {}, "/videos/1")).rejects.toBeInstanceOf(ApiError);
    expect(handler).toHaveBeenCalledWith("/videos/1");
  });
});
