import { beforeEach, describe, expect, it } from "vitest";
import { authStore } from "./authStore";

beforeEach(() => { sessionStorage.clear(); authStore.clear(); });

describe("authStore", () => {
  it("persiste el JWT en sessionStorage y lo puede limpiar", () => {
    const user = { id: "1", username: "ana", email: "a@test", quotaBytes: 10, usedBytes: 0 };
    authStore.setSession("jwt", user);
    expect(sessionStorage.getItem("tecflix.jwt")).toBe("jwt");
    expect(authStore.getUser()).toEqual(user);
    authStore.clear();
    expect(sessionStorage.getItem("tecflix.jwt")).toBeNull();
  });
});
