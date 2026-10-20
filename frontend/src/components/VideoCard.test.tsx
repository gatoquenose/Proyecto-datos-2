import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { authStore } from "../auth/authStore";
import { VideoCard } from "./VideoCard";

const observerCallbacks: IntersectionObserverCallback[] = [];

class MockIntersectionObserver {
  constructor(callback: IntersectionObserverCallback) {
    observerCallbacks.push(callback);
  }
  observe(): void {}
  disconnect(): void {}
  unobserve(): void {}
}

const video = {
  id: "550e8400-e29b-41d4-a716-446655440001",
  ownerId: "550e8400-e29b-41d4-a716-446655440000",
  ownerUsername: "ana.tec",
  title: "Video",
  description: "Descripción",
  author: "Ana",
  topic: "Tema",
  tags: ["tag"],
  durationSeconds: 60,
  uploadedAt: "2026-10-19T22:00:00Z",
  visibility: "public" as const,
  sizeBytes: 1000,
  status: "ready" as const,
  createdAt: "2026-10-19T21:00:00Z",
  updatedAt: "2026-10-19T22:00:00Z",
};

describe("VideoCard", () => {
  beforeEach(() => {
    observerCallbacks.length = 0;
    authStore.setSession("test-token", {
      id: video.ownerId,
      username: "ana.tec",
      email: "ana@example.test",
      quotaBytes: 100,
      usedBytes: 10,
    });
    vi.stubGlobal("IntersectionObserver", MockIntersectionObserver);
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(new Blob(["image"]), { status: 200 })));
  });

  it("descarga la miniatura solo cuando la tarjeta entra en el viewport", async () => {
    render(<MemoryRouter><VideoCard video={video} showOwnerDetails={false} /></MemoryRouter>);
    expect(fetch).not.toHaveBeenCalled();

    observerCallbacks[0]([{ isIntersecting: true } as IntersectionObserverEntry], {} as IntersectionObserver);
    await waitFor(() => expect(fetch).toHaveBeenCalled());
  });

  it("muestra marcador si la miniatura no está disponible", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(null, { status: 404 })));
    render(<MemoryRouter><VideoCard video={video} showOwnerDetails={false} /></MemoryRouter>);
    observerCallbacks[0]([{ isIntersecting: true } as IntersectionObserverEntry], {} as IntersectionObserver);
    expect(await screen.findByText("Miniatura no disponible")).toBeInTheDocument();
  });
});
