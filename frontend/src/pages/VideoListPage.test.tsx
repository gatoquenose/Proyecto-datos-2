import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { VideoListPage } from "./VideoListPage";

const metadata = vi.hoisted(() => ({ listVideos: vi.fn() }));
vi.mock("../api/metadataClient", () => ({ metadataClient: metadata }));
vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ user: { id: "owner-1" } }) }));

const video = (id: string, title: string, ownerId = "owner-1") => ({
  id,
  ownerId,
  ownerUsername: ownerId === "owner-1" ? "ana.tec" : "autor.2",
  title,
  description: "Descripción",
  author: "Autor",
  topic: "Tema",
  tags: ["tag"],
  durationSeconds: 125,
  uploadedAt: "2026-10-19T22:00:00Z",
  visibility: "private" as const,
  sizeBytes: 1000,
  status: "ready" as const,
  createdAt: "2026-10-19T22:00:00Z",
  updatedAt: "2026-10-19T22:00:00Z",
});

describe("VideoListPage", () => {
  beforeEach(() => {
    metadata.listVideos.mockReset();
  });

  it("lista videos propios con owner=me y muestra sus metadatos", async () => {
    metadata.listVideos.mockResolvedValueOnce({ items: [video("1", "Video propio")], nextCursor: null });
    renderPage("/videos/mine");
    expect(await screen.findByRole("heading", { name: "Mis videos" })).toBeInTheDocument();
    expect(screen.getByRole("heading", { name: "Video propio" })).toBeInTheDocument();
    expect(screen.queryByText("Público")).not.toBeInTheDocument();
    expect(screen.getByText("Privado")).toBeInTheDocument();
    expect(screen.getByText("Listo")).toBeInTheDocument();
    expect(metadata.listVideos).toHaveBeenCalledWith({ owner: "me", sortBy: "uploadedAt", order: "desc", q: undefined, pageSize: 12, cursor: undefined });
  });

  it("acumula páginas al pulsar Cargar más", async () => {
    metadata.listVideos
      .mockResolvedValueOnce({ items: [video("1", "Primera página")], nextCursor: "cursor-2" })
      .mockResolvedValueOnce({ items: [video("2", "Segunda página", "owner-2")], nextCursor: null });
    renderPage("/videos");
    expect(await screen.findByRole("heading", { name: "Primera página" })).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "Cargar más" }));
    expect(await screen.findByRole("heading", { name: "Segunda página" })).toBeInTheDocument();
    expect(metadata.listVideos).toHaveBeenLastCalledWith({ sortBy: "uploadedAt", order: "desc", q: undefined, pageSize: 12, cursor: "cursor-2" });
  });

  it("aplica debounce a la búsqueda y reinicia el cursor al cambiar el orden", async () => {
    metadata.listVideos
      .mockResolvedValueOnce({ items: [video("1", "Inicial")], nextCursor: null })
      .mockResolvedValueOnce({ items: [video("2", "Resultado")], nextCursor: null })
      .mockResolvedValueOnce({ items: [video("3", "Ordenado")], nextCursor: null });
    renderPage("/videos");
    await screen.findByRole("heading", { name: "Inicial" });
    fireEvent.change(screen.getByRole("searchbox", { name: "Buscar videos" }), { target: { value: "resultado" } });
    await waitFor(() => expect(metadata.listVideos).toHaveBeenLastCalledWith({ sortBy: "uploadedAt", order: "desc", q: "resultado", pageSize: 12, cursor: undefined }), { timeout: 1000 });
    fireEvent.change(screen.getByLabelText("Ordenar por"), { target: { value: "title" } });
    await waitFor(() => expect(metadata.listVideos).toHaveBeenLastCalledWith({ sortBy: "title", order: "desc", q: "resultado", pageSize: 12, cursor: undefined }));
  });
});

function renderPage(path: string): void {
  const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  render(<QueryClientProvider client={client}><MemoryRouter initialEntries={[path]}><VideoListPage /></MemoryRouter></QueryClientProvider>);
}
