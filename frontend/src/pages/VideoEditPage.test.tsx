import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter, Route, Routes, useLocation } from "react-router-dom";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { ApiError } from "../types/api";
import { VideoEditPage } from "./VideoEditPage";

const api = vi.hoisted(() => ({ getVideo: vi.fn(), updateVideo: vi.fn(), updateVideoVisibility: vi.fn() }));
const auth = vi.hoisted(() => ({ user: { id: "owner-1" } }));

vi.mock("../api/metadataClient", () => ({ metadataClient: api }));
vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ user: auth.user }) }));

const video = {
  id: "video-1",
  ownerId: "owner-1",
  ownerUsername: "owner",
  title: "Clase",
  description: "Descripción",
  author: "Ana",
  topic: "Algoritmos",
  tags: ["tag"],
  durationSeconds: 125,
  uploadedAt: "2026-10-19T22:00:00Z",
  visibility: "private" as const,
  sizeBytes: 1_000_000,
  status: "ready" as const,
  createdAt: "2026-10-19T22:00:00Z",
  updatedAt: "2026-10-19T22:00:00Z",
};

beforeEach(() => {
  api.getVideo.mockReset();
  api.updateVideo.mockReset().mockResolvedValue(video);
  api.updateVideoVisibility.mockReset().mockResolvedValue(video);
  auth.user = { id: "owner-1" };
});

afterEach(() => {
  vi.restoreAllMocks();
});

describe("VideoEditPage", () => {
  it("carga y muestra el formulario al dueño", async () => {
    api.getVideo.mockResolvedValue(video);
    renderEditPage();

    expect(await screen.findByRole("heading", { name: "Editar video" })).toBeInTheDocument();
    expect(screen.getByLabelText("Título")).toHaveValue("Clase");
    expect(api.getVideo).toHaveBeenCalledWith("video-1");
  });

  it("muestra el estado de carga mientras obtiene el video", () => {
    api.getVideo.mockReturnValue(new Promise(() => undefined));
    renderEditPage();

    expect(screen.getByText("Cargando video…")).toBeInTheDocument();
  });

  it("redirige a la lista si el usuario no es dueño", async () => {
    auth.user = { id: "other-user" };
    api.getVideo.mockResolvedValue(video);
    renderEditPage();

    await waitFor(() => expect(screen.getByTestId("location")).toHaveTextContent("/videos"));
    expect(screen.queryByRole("heading", { name: "Editar video" })).not.toBeInTheDocument();
  });

  it("redirige a la lista cuando el servidor responde 404", async () => {
    api.getVideo.mockRejectedValue(new ApiError(404, {
      code: "NOT_FOUND",
      message: "No encontramos el video solicitado.",
      details: null,
      traceId: "test",
    }));
    renderEditPage();

    await waitFor(() => expect(screen.getByTestId("location")).toHaveTextContent("/videos"));
  });

  it("guarda los cambios y navega al detalle", async () => {
    api.getVideo.mockResolvedValue(video);
    const client = renderEditPage();

    const title = await screen.findByLabelText("Título");
    fireEvent.change(title, { target: { value: "Nueva clase" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await waitFor(() => expect(api.updateVideo).toHaveBeenCalledWith("video-1", { title: "Nueva clase" }));
    await waitFor(() => expect(screen.getByTestId("location")).toHaveTextContent("/videos/video-1"));
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["videos"] });
  });
});

function renderEditPage(): QueryClient {
  const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  vi.spyOn(client, "invalidateQueries");
  render(
    <QueryClientProvider client={client}>
      <MemoryRouter initialEntries={["/videos/video-1/edit"]}>
        <Routes>
          <Route path="/videos/:id/edit" element={<VideoEditPage />} />
          <Route path="/videos/:id" element={<p>Detalle del video</p>} />
          <Route path="/videos" element={<p>Lista de videos</p>} />
          <Route path="*" element={<p>Otra página</p>} />
        </Routes>
        <LocationProbe />
      </MemoryRouter>
    </QueryClientProvider>,
  );
  return client;
}

function LocationProbe(): JSX.Element {
  const location = useLocation();
  return <output data-testid="location">{location.pathname}</output>;
}
