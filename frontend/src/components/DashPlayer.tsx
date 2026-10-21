import { useEffect, useRef, useState } from "react";
import { notifyUnauthorized } from "../api/httpClient";
import { authStore } from "../auth/authStore";

export interface VariantTrack {
  id: number;
  height?: number;
  width?: number;
}

export interface PlayerLike {
  getVariantTracks: () => VariantTrack[];
  selectVariantTrack: (track: VariantTrack) => void;
  configure: (config: Record<string, unknown>) => void;
  load: (url: string) => Promise<void>;
  destroy: () => Promise<void>;
  addEventListener?: (name: string, listener: (event: Event) => void) => void;
  removeEventListener?: (name: string, listener: (event: Event) => void) => void;
  getNetworkingEngine?: () => { registerRequestFilter: (filter: (type: number, request: { headers: Record<string, string> }) => void) => void };
}

export const uniqueQualityTracks = (tracks: VariantTrack[]): VariantTrack[] => {
  const byHeight = new Map<number, VariantTrack>();
  tracks.filter((track) => typeof track.height === "number").forEach((track) => {
    if (!byHeight.has(track.height!)) byHeight.set(track.height!, track);
  });
  return [...byHeight.values()].sort((left, right) => (left.height ?? 0) - (right.height ?? 0));
};

export const selectQuality = (player: PlayerLike, height: number | "auto"): void => {
  if (height === "auto") {
    player.configure({ abr: { enabled: true } });
    return;
  }
  const track = uniqueQualityTracks(player.getVariantTracks()).find((candidate) => candidate.height === height);
  if (!track) return;
  player.configure({ abr: { enabled: false } });
  player.selectVariantTrack(track);
};

export const statusFromShakaError = (error: { category?: number; code?: number; data?: unknown }): number | null => {
  if (error.code !== 1001) return null;
  if (typeof error.data === "number") return error.data;
  if (Array.isArray(error.data)) return error.data.find((value): value is number => typeof value === "number" && value >= 100 && value <= 599) ?? null;
  if (typeof error.data === "object" && error.data !== null && "status" in error.data) {
    const status = error.data.status;
    return typeof status === "number" ? status : null;
  }
  return null;
};

export const shakaErrorMessage = (error: { category?: number; code?: number; data?: unknown }): string => {
  const status = statusFromShakaError(error);
  if (status === 401) return "Tu sesión expiró. Inicia sesión de nuevo.";
  if (status === 403) return "No tienes permiso para reproducir este video.";
  if (status === 404) return "No encontramos el manifiesto o segmento del video.";
  return "No se pudo reproducir el video.";
};

export interface ShakaModule {
  default: {
    polyfill: { installAll: () => void };
    Player: {
      new (video: HTMLVideoElement): PlayerLike;
      isBrowserSupported: () => boolean;
    };
  };
}

export function DashPlayer({ manifestUrl, onError, loadShaka }: { manifestUrl: string; onError?: (message: string) => void; loadShaka?: () => Promise<ShakaModule> }): JSX.Element {
  const videoRef = useRef<HTMLVideoElement>(null);
  const playerRef = useRef<PlayerLike | null>(null);
  const [tracks, setTracks] = useState<VariantTrack[]>([]);
  const [quality, setQuality] = useState<"auto" | number>("auto");
  const [message, setMessage] = useState("");

  useEffect(() => {
    let cancelled = false;
    const initialize = async (): Promise<void> => {
      const shaka = await (loadShaka ? loadShaka() : import("shaka-player") as unknown as Promise<ShakaModule>);
      shaka.default.polyfill.installAll();
      if (!shaka.default.Player.isBrowserSupported()) {
        setMessage("Tu navegador no soporta la reproducción DASH.");
        return;
      }
      if (!videoRef.current || cancelled) return;
      const player = new shaka.default.Player(videoRef.current);
      playerRef.current = player;
      player.configure({ abr: { enabled: true } });
      player.configure({ streaming: { retryParameters: { maxAttempts: 1 } } });
      player.getNetworkingEngine?.().registerRequestFilter((_type, request) => {
        const token = authStore.getToken();
        if (!token) return;
        request.headers.Authorization = `Bearer ${token}`;
      });
      player.addEventListener?.("error", (event) => {
        const error = (event as unknown as { detail?: { category?: number; code?: number; data?: unknown } }).detail;
        const nextMessage = shakaErrorMessage(error ?? {});
        setMessage(nextMessage);
        onError?.(nextMessage);
        if (statusFromShakaError(error ?? {}) === 401) notifyUnauthorized();
      });
      const token = authStore.getToken();
      if (!token) {
        setMessage("Tu sesión expiró. Inicia sesión de nuevo.");
        return;
      }
      await player.load(manifestUrl);
      if (cancelled) return;
      setTracks(uniqueQualityTracks(player.getVariantTracks()));
    };
    void initialize().catch((error: unknown) => {
      const nextMessage = error instanceof Error ? error.message : "No se pudo cargar el reproductor.";
      setMessage(nextMessage);
      onError?.(nextMessage);
    });
    return () => {
      cancelled = true;
      void playerRef.current?.destroy();
      playerRef.current = null;
    };
  }, [loadShaka, manifestUrl, onError]);

  return <div className="dash-player">
    {/* eslint-disable-next-line jsx-a11y/media-has-caption -- v1 no ofrece subtítulos; se documenta como limitación F4. */}
    <video ref={videoRef} controls playsInline aria-label="Reproductor de video" />
    {message && <p className="error-box" role="alert">{message}</p>}
    {tracks.length > 0 && <label htmlFor="video-quality">Calidad
      <select id="video-quality" value={quality} onChange={(event) => {
        const value = event.target.value === "auto" ? "auto" : Number(event.target.value);
        setQuality(value);
        if (playerRef.current) selectQuality(playerRef.current, value);
      }}>
        <option value="auto">Auto</option>
        {tracks.map((track) => <option key={track.id} value={track.height}>{track.height}p</option>)}
      </select>
    </label>}
  </div>;
}
