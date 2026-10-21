import { afterEach, describe, expect, it, vi } from "vitest";
import { usersClient } from "./usersClient";
import { setHttpToken } from "./httpClient";

afterEach(() => { vi.restoreAllMocks(); setHttpToken(null); });

describe("usersClient", () => {
  it("envía únicamente email al actualizar usuario", async () => {
    const fetchMock = vi.fn().mockResolvedValue(new Response(JSON.stringify({ id: "1", username: "ana", email: "new@test.com", quotaBytes: 100, usedBytes: 0 }), { status: 200 }));
    vi.stubGlobal("fetch", fetchMock);
    setHttpToken("test-token");
    await usersClient.updateUser("1", { email: "new@test.com" });
    const init = fetchMock.mock.calls[0][1] as RequestInit;
    expect(JSON.parse(String(init.body))).toEqual({ email: "new@test.com" });
  });
});
