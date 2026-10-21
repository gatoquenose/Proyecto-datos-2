import { describe, expect, it, vi } from "vitest";
import { selectQuality, shakaErrorMessage, statusFromShakaError, uniqueQualityTracks, type PlayerLike } from "./DashPlayer";

describe("DashPlayer helpers", () => {
  it("deduplica calidades por altura", () => {
    expect(uniqueQualityTracks([{ id: 1, height: 720 }, { id: 2, height: 360 }, { id: 3, height: 720 }])).toEqual([{ id: 2, height: 360 }, { id: 1, height: 720 }]);
  });

  it("desactiva ABR al elegir calidad y lo reactiva en Auto", () => {
    const player: PlayerLike = { getVariantTracks: () => [{ id: 1, height: 360 }], selectVariantTrack: vi.fn(), configure: vi.fn(), load: vi.fn(), destroy: vi.fn() };
    selectQuality(player, 360);
    expect(player.configure).toHaveBeenCalledWith({ abr: { enabled: false } });
    expect(player.selectVariantTrack).toHaveBeenCalledWith({ id: 1, height: 360 });
    selectQuality(player, "auto");
    expect(player.configure).toHaveBeenLastCalledWith({ abr: { enabled: true } });
  });

  it.each([401, 403, 404])("mapea BAD_HTTP_STATUS real con status %s", (status) => {
    const error = { category: 1, code: 1001, data: ["manifest", status] };
    expect(statusFromShakaError(error)).toBe(status);
    expect(shakaErrorMessage(error)).not.toBe("No se pudo reproducir el video.");
  });
});
