import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter, Route, Routes, useLocation } from "react-router-dom";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { VideoDetailPage } from "./VideoDetailPage";
import { ApiError } from "../types/api";

const metadata = vi.hoisted(() => ({ getVideo: vi.fn(), updateVideoVisibility: vi.fn(), deleteVideo: vi.fn() }));
const streaming = vi.hoisted(() => ({ getProcessing: vi.fn(), getManifestUrl: vi.fn(() => "/manifest.mpd") }));
const auth = vi.hoisted(() => ({ user: { id: "owner-1" } }));

vi.mock("../api/metadataClient", () => ({ metadataClient: metadata }));
vi.mock("../api/streamingClient", () => ({ streamingClient: streaming }));
vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ user: auth.user }) }));
vi.mock("../components/DashPlayer", () => ({ DashPlayer: () => <div data-testid="dash-player" /> }));

const video = (status: "ready" | "processing" = "ready", ownerId = "owner-1") => ({
  id: "video-1", ownerId, ownerUsername: "ana.tec", title: "Clase", description: "Descripción", author: "Ana",
  topic: "Algoritmos", tags: ["tag"], durationSeconds: 12, uploadedAt: "2026-10-19T22:00:00Z",
  visibility: "private" as const, sizeBytes: 1000, status, createdAt: "2026-10-19T22:00:00Z", updatedAt: "2026-10-19T22:00:00Z",
});

beforeEach(() => {
  metadata.getVideo.mockReset();
  metadata.updateVideoVisibility.mockReset().mockResolvedValue({ ...video(), visibility: "public" });
  metadata.deleteVideo.mockReset().mockResolvedValue(undefined);
  streaming.getProcessing.mockReset();
  auth.user = { id: "owner-1" };
});

afterEach(() => {
  vi.useRealTimers();
});

describe("VideoDetailPage", () => {
  it("sondea processing y se detiene en ready, invalidando video y lista", async () => {
    vi.useFakeTimers();
    metadata.getVideo.mockResolvedValue(video("processing"));
    streaming.getProcessing.mockResolvedValueOnce({ videoId: "video-1", status: "processing" }).mockResolvedValueOnce({ videoId: "video-1", status: "ready" });
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    const invalidateQueries = vi.spyOn(client, "invalidateQueries");
    renderPage(client);
    await act(async () => { await vi.advanceTimersByTimeAsync(0); await Promise.resolve(); await Promise.resolve(); await Promise.resolve(); });
    expect(streaming.getProcessing).toHaveBeenCalledTimes(1);
    await act(async () => { await vi.advanceTimersByTimeAsync(2000); });
    await act(async () => { await Promise.resolve(); await Promise.resolve(); });
    expect(client.getQueryState(["processing", "video-1"])?.data).toMatchObject({ status: "ready" });
    expect(invalidateQueries).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
    expect(invalidateQueries).toHaveBeenCalledWith({ queryKey: ["videos"] });
  });

  it.each(["failed", "forbidden", "not-found"])("detiene el sondeo en %s", async (outcome) => {
    metadata.getVideo.mockResolvedValue(video("processing"));
    if (outcome === "failed") streaming.getProcessing.mockResolvedValue({ videoId: "video-1", status: "failed", error: "falló" });
    else streaming.getProcessing.mockRejectedValue(new ApiError(outcome === "forbidden" ? 403 : 404, { code: outcome === "forbidden" ? "FORBIDDEN" : "NOT_FOUND", message: "error", details: null, traceId: "test" }));
    const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
    renderPage(client);
    await waitFor(() => expect(streaming.getProcessing).toHaveBeenCalledTimes(1));
    vi.useFakeTimers();
    await act(async () => { await vi.advanceTimersByTimeAsync(4000); });
    expect(streaming.getProcessing).toHaveBeenCalledTimes(1);
  });

  it("solo muestra acciones al dueño, cambia visibilidad y exige confirmación al eliminar", async () => {
    metadata.getVideo.mockResolvedValue(video());
    renderPage(new QueryClient({ defaultOptions: { queries: { retry: false } } }));
    expect(await screen.findByRole("link", { name: "Editar" })).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: /Cambiar a público/ }));
    await waitFor(() => expect(metadata.updateVideoVisibility).toHaveBeenCalledWith("video-1", "public"));
    fireEvent.click(screen.getByRole("button", { name: "Eliminar" }));
    expect(screen.getByRole("dialog")).toBeInTheDocument();
    expect(metadata.deleteVideo).not.toHaveBeenCalled();
    fireEvent.click(screen.getByRole("button", { name: "Eliminar video" }));
    await waitFor(() => expect(metadata.deleteVideo).toHaveBeenCalledWith("video-1"));
    expect(screen.getByTestId("location")).toHaveTextContent("/videos/mine");
  });

  it("oculta las acciones a un usuario que no es dueño", async () => {
    auth.user = { id: "other-user" };
    metadata.getVideo.mockResolvedValue(video("ready", "owner-1"));
    renderPage(new QueryClient({ defaultOptions: { queries: { retry: false } } }));
    await screen.findByRole("heading", { name: "Clase" });
    expect(screen.queryByRole("link", { name: "Editar" })).not.toBeInTheDocument();
    expect(screen.queryByRole("button", { name: "Eliminar" })).not.toBeInTheDocument();
  });
});

function renderPage(client: QueryClient): void {
  render(<QueryClientProvider client={client}><MemoryRouter initialEntries={["/videos/video-1"]}><Routes><Route path="/videos/:id" element={<><VideoDetailPage /><LocationProbe /></>} /></Routes></MemoryRouter></QueryClientProvider>);
}

function LocationProbe(): JSX.Element {
  const location = useLocation();
  return <output data-testid="location">{location.pathname}</output>;
}
