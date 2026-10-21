import { act, render, screen, waitFor } from "@testing-library/react";
import { StrictMode } from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { authStore } from "../auth/authStore";
import { setUnauthorizedHandler } from "../api/httpClient";
import { DashPlayer, type ShakaModule } from "./DashPlayer";

const shaka = vi.hoisted(() => {
  const players: Array<{
    configure: ReturnType<typeof vi.fn>;
    destroy: ReturnType<typeof vi.fn>;
    load: ReturnType<typeof vi.fn>;
    getVariantTracks: ReturnType<typeof vi.fn>;
    selectVariantTrack: ReturnType<typeof vi.fn>;
    addEventListener: ReturnType<typeof vi.fn>;
    filter?: (type: number, request: { headers: Record<string, string> }) => void;
  }> = [];
  const Player = vi.fn(() => {
    const player = {
      configure: vi.fn(),
      destroy: vi.fn().mockResolvedValue(undefined),
      load: vi.fn().mockResolvedValue(undefined),
      getVariantTracks: vi.fn(() => [{ id: 1, height: 360 }, { id: 2, height: 720 }]),
      selectVariantTrack: vi.fn(),
      addEventListener: vi.fn(),
      getNetworkingEngine: () => ({ registerRequestFilter: (filter: (type: number, request: { headers: Record<string, string> }) => void) => { player.filter = filter; } }),
    };
    players.push(player);
    return player;
  });
  return { players, default: { polyfill: { installAll: vi.fn() }, Player: Object.assign(Player, { isBrowserSupported: vi.fn(() => true) }) } };
});

vi.mock("shaka-player", () => shaka);

beforeEach(() => {
  shaka.players.length = 0;
  shaka.default.Player.isBrowserSupported.mockReturnValue(true);
  authStore.setSession("test-token", { id: "user-1", username: "ana.tec", email: "ana@example.test", quotaBytes: 100, usedBytes: 0 });
});

afterEach(() => {
  authStore.clear();
  setUnauthorizedHandler(null);
  vi.clearAllMocks();
});

describe("DashPlayer", () => {
  it("agrega Authorization al manifiesto y segmentos sin enviar Bearer null", async () => {
    const { unmount } = render(<DashPlayer manifestUrl="/manifest.mpd" />);
    await waitFor(() => expect(shaka.players).toHaveLength(1));
    const request = { headers: {} };
    shaka.players[0].filter?.(0, request);
    expect(request.headers.Authorization).toBe("Bearer test-token");
    authStore.clear();
    const unauthenticatedRequest = { headers: {} };
    shaka.players[0].filter?.(0, unauthenticatedRequest);
    expect(unauthenticatedRequest.headers).not.toHaveProperty("Authorization");
    unmount();
  });

  it("destruye el reproductor al desmontar y no duplica instancias en StrictMode", async () => {
    shaka.default.Player.isBrowserSupported.mockImplementation(() => true);
    expect(shaka.default.Player.isBrowserSupported()).toBe(true);
    const loadShaka = () => Promise.resolve(shaka as unknown as ShakaModule);
    const { unmount } = render(<StrictMode><DashPlayer manifestUrl="/manifest.mpd" loadShaka={loadShaka} /></StrictMode>);
    await waitFor(() => expect(shaka.players).toHaveLength(1));
    unmount();
    expect(shaka.players[0].destroy).toHaveBeenCalledTimes(1);
  });

  it("dispara sesión expirada con returnTo ante 401", async () => {
    const unauthorized = vi.fn();
    setUnauthorizedHandler(unauthorized);
    render(<DashPlayer manifestUrl="/manifest.mpd" />);
    await waitFor(() => expect(shaka.players).toHaveLength(1));
    const listener = shaka.players[0].addEventListener.mock.calls.find(([name]) => name === "error")?.[1] as (event: Event) => void;
    act(() => listener({ detail: { category: 1, code: 1001, data: ["manifest", 401] } } as Event));
    expect(unauthorized).toHaveBeenCalledWith(window.location.pathname + window.location.search);
    expect(screen.getByRole("alert")).toHaveTextContent("Tu sesión expiró");
  });

  it.each([[403, "No tienes permiso"], [404, "No encontramos"]])("muestra %s sin reintentos", async (status, message) => {
    render(<DashPlayer manifestUrl="/manifest.mpd" />);
    await waitFor(() => expect(shaka.players).toHaveLength(1));
    expect(shaka.players[0].configure).toHaveBeenCalledWith({ streaming: { retryParameters: { maxAttempts: 1 } } });
    const listener = shaka.players[0].addEventListener.mock.calls.find(([name]) => name === "error")?.[1] as (event: Event) => void;
    act(() => listener({ detail: { category: 1, code: 1001, data: ["segment", status] } } as Event));
    expect(screen.getByRole("alert")).toHaveTextContent(message);
  });
});
