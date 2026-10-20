import { useEffect, useState } from "react";
import { authStore } from "../auth/authStore";

const streamingBaseUrl = import.meta.env.VITE_STREAMING_API_URL ?? "http://localhost:8083/api/v1";

interface AuthedImageState {
  url: string | null;
  failed: boolean;
}

export function useAuthedImage(videoId: string, enabled: boolean): AuthedImageState {
  const [state, setState] = useState<AuthedImageState>({ url: null, failed: false });

  useEffect(() => {
    if (!enabled) return;
    const controller = new AbortController();
    let objectUrl: string | null = null;

    const load = async (): Promise<void> => {
      try {
        const token = authStore.getToken();
        const response = await fetch(`${streamingBaseUrl}/videos/${videoId}/thumbnail`, {
          signal: controller.signal,
          headers: token ? { Authorization: `Bearer ${token}` } : {},
        });
        if (!response.ok) throw new Error(`Thumbnail request failed: ${response.status}`);
        const blob = await response.blob();
        objectUrl = URL.createObjectURL(blob);
        setState({ url: objectUrl, failed: false });
      } catch (error) {
        if (controller.signal.aborted) return;
        setState({ url: null, failed: true });
      }
    };

    void load();
    return () => {
      controller.abort();
      if (objectUrl) URL.revokeObjectURL(objectUrl);
    };
  }, [videoId, enabled]);

  return state;
}
