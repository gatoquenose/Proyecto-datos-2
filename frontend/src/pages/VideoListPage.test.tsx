import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter, useLocation, useNavigate } from "react-router-dom";
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
      .mockResolvedValueOnce({ items: [video("1", "Duplicado"), video("2", "Segunda página", "owner-2")], nextCursor: null });
    renderPage("/videos");
    expect(await screen.findByRole("heading", { name: "Primera página" })).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "Cargar más" }));
    expect(await screen.findByRole("heading", { name: "Segunda página" })).toBeInTheDocument();
    expect(screen.getAllByRole("heading", { name: /Primera página|Duplicado/ })).toHaveLength(1);
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
    expect(await screen.findByRole("heading", { name: "Resultado" })).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText("Ordenar por"), { target: { value: "title" } });
    await waitFor(() => expect(metadata.listVideos).toHaveBeenLastCalledWith({ sortBy: "title", order: "desc", q: "resultado", pageSize: 12, cursor: undefined }));
  });

  it("restaura q, sortBy y order desde la URL y anuncia el conteo", async () => {
    metadata.listVideos.mockResolvedValueOnce({ items: [video("1", "Resultado")], nextCursor: null });
    renderPage("/videos?q=resultado&sortBy=title&order=asc");
    expect(await screen.findByRole("heading", { name: "Resultado" })).toBeInTheDocument();
    expect(screen.getByRole("searchbox", { name: "Buscar videos" })).toHaveValue("resultado");
    expect(screen.getByLabelText("Ordenar por")).toHaveValue("title");
    expect(screen.getByLabelText("Dirección")).toHaveValue("asc");
    expect(screen.getByText("1 video encontrado")).toBeInTheDocument();
    expect(metadata.listVideos).toHaveBeenCalledWith({ sortBy: "title", order: "asc", q: "resultado", pageSize: 12, cursor: undefined });
  });

  it("restaura los parámetros anteriores al pulsar Atrás", async () => {
    metadata.listVideos
      .mockResolvedValueOnce({ items: [video("1", "Filtrado")], nextCursor: null })
      .mockResolvedValueOnce({ items: [video("2", "Inicial")], nextCursor: null });
    renderPage(["/videos", "/videos?q=filtrado&sortBy=title&order=asc"], 1);
    expect(await screen.findByRole("heading", { name: "Filtrado" })).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "Atrás" }));
    expect(await screen.findByRole("heading", { name: "Inicial" })).toBeInTheDocument();
    expect(screen.getByRole("searchbox", { name: "Buscar videos" })).toHaveValue("");
    expect(metadata.listVideos).toHaveBeenLastCalledWith({ owner: undefined, sortBy: "uploadedAt", order: "desc", q: undefined, pageSize: 12, cursor: undefined });
  });

  it("muestra estado vacío y permite reintentar un error", async () => {
    metadata.listVideos
      .mockRejectedValueOnce(new Error("network"))
      .mockResolvedValueOnce({ items: [], nextCursor: null });
    renderPage("/videos");
    expect(await screen.findByRole("alert")).toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "Reintentar" }));
    expect(await screen.findByText("No hay videos para mostrar.")).toBeInTheDocument();
    expect(screen.getByText("0 videos encontrados")).toBeInTheDocument();
  });
});

function renderPage(path: string | string[], initialIndex?: number): void {
  const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  const initialEntries = Array.isArray(path) ? path : [path];
  render(<QueryClientProvider client={client}><MemoryRouter initialEntries={initialEntries} initialIndex={initialIndex}><VideoListPage /><LocationProbe /></MemoryRouter></QueryClientProvider>);
}

function LocationProbe(): JSX.Element {
  const location = useLocation();
  const navigate = useNavigate();
  return <><output data-testid="location">{location.search}</output><button type="button" onClick={() => navigate(-1)}>Atrás</button></>;
}
