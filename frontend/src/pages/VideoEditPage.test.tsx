import { StrictMode } from "react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { act, cleanup, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter, Route, Routes, useLocation } from "react-router-dom";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { AppLayout } from "../components/AppLayout";
import { ProtectedRoute } from "../components/ProtectedRoute";
import { AuthProvider, useAuth } from "../auth/AuthContext";
import { authStore } from "../auth/authStore";
import { request } from "../api/httpClient";
import { ApiError } from "../types/api";
import { LoginPage } from "./LoginPage";
import { VideoEditPage } from "./VideoEditPage";

const api = vi.hoisted(() => ({ getVideo: vi.fn(), updateVideo: vi.fn(), updateVideoVisibility: vi.fn() }));
const auth = vi.hoisted(() => ({ user: { id: "owner-1" } }));
const users = vi.hoisted(() => ({ getCurrentUser: vi.fn(), login: vi.fn() }));

vi.mock("../api/metadataClient", () => ({ metadataClient: api }));
vi.mock("../api/usersClient", () => ({ usersClient: users }));

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
  users.getCurrentUser.mockReset().mockImplementation(async () => ({
    id: auth.user.id,
    username: "owner",
    email: "owner@example.test",
  }));
  users.login.mockReset();
  authStore.clear();
});

afterEach(() => {
  cleanup();
  vi.restoreAllMocks();
  authStore.clear();
});

describe("VideoEditPage", () => {
  it("carga y muestra el formulario al dueño", async () => {
    api.getVideo.mockResolvedValue(video);
    await renderEditPage();

    expect(await screen.findByRole("heading", { name: "Editar video" })).toBeInTheDocument();
    expect(screen.getByLabelText("Título")).toHaveValue("Clase");
    expect(api.getVideo).toHaveBeenCalledWith("video-1");
  });

  it("muestra el estado de carga mientras obtiene el video", async () => {
    api.getVideo.mockReturnValue(new Promise(() => undefined));
    await renderEditPage();

    expect(screen.getByText("Cargando video…")).toBeInTheDocument();
  });

  it("redirige a la lista si el usuario no es dueño", async () => {
    auth.user = { id: "other-user" };
    api.getVideo.mockResolvedValue(video);
    await renderEditPage();

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
    await renderEditPage();

    await waitFor(() => expect(screen.getByTestId("location")).toHaveTextContent("/videos"));
  });

  it("guarda los cambios y navega al detalle", async () => {
    api.getVideo.mockResolvedValue(video);
    const client = await renderEditPage();

    const title = await screen.findByLabelText("Título");
    fireEvent.change(title, { target: { value: "Nueva clase" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await waitFor(() => expect(api.updateVideo).toHaveBeenCalledWith("video-1", { title: "Nueva clase" }));
    await waitFor(() => expect(screen.getByTestId("location")).toHaveTextContent("/videos/video-1"));
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["videos"] });
  });

  it("redirige a la lista con el mensaje de permisos ante un 403", async () => {
    api.getVideo.mockRejectedValue(new ApiError(403, {
      code: "FORBIDDEN",
      message: "No tienes permisos para editar este video.",
      details: null,
      traceId: "test",
    }));
    await renderEditPage();

    expect(await screen.findByText("No tienes permisos para editar este video.")).toBeInTheDocument();
    expect(screen.getByTestId("location")).toHaveTextContent("/videos");
  });

  it("muestra el mensaje propio del servicio ante un error 5xx", async () => {
    api.getVideo.mockRejectedValue(new ApiError(503, {
      code: "SERVICE_UNAVAILABLE",
      message: "Servicio de metadata no disponible.",
      details: null,
      traceId: "test",
    }));
    await renderEditPage();

    expect(await screen.findByRole("alert")).toHaveTextContent("El servicio no está listo.");
  });

  it("muestra sesión expirada y conserva returnTo cuando metadata responde 401", async () => {
    api.getVideo.mockResolvedValue(video);
    const returnTo = "/videos/video-1/edit";
    api.updateVideo.mockImplementation(() =>
      request("/protected-video-update", { requiresAuth: true }, returnTo));
    vi.spyOn(globalThis, "fetch").mockResolvedValueOnce(new Response(JSON.stringify({
      code: "TOKEN_EXPIRED",
      message: "Token expirado.",
      details: null,
      traceId: "test",
    }), { status: 401, headers: { "Content-Type": "application/json" } }));
    await renderEditPage(returnTo);
    fireEvent.change(await screen.findByLabelText("Título"), { target: { value: "Nueva clase" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await screen.findByRole("heading", { name: "Iniciar sesión" });
    expect(await screen.findByRole("alert")).toHaveTextContent("Tu sesión expiró.");
    expect(screen.getByTestId("location")).toHaveTextContent("/login");
    expect(screen.getByTestId("return-to")).toHaveTextContent(returnTo);
  });
});

async function renderEditPage(initialEntry = "/videos/video-1/edit"): Promise<QueryClient> {
  const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  vi.spyOn(client, "invalidateQueries");
  authStore.setSession("test-token", {
    id: auth.user.id,
    username: "owner",
    email: "owner@example.test",
    quotaBytes: 100,
    usedBytes: 1,
  });
  const restoredUser = {
    id: auth.user.id,
    username: "owner",
    email: "owner@example.test",
  };
  let resolveCurrentUser!: (user: typeof restoredUser) => void;
  const currentUserPromise = new Promise<typeof restoredUser>((resolve) => {
    resolveCurrentUser = resolve;
  });
  users.getCurrentUser.mockReset().mockReturnValue(currentUserPromise);
  render(
    <StrictMode>
      <MemoryRouter initialEntries={[initialEntry]}>
        <AuthProvider>
          <QueryClientProvider client={client}>
          <Routes>
            <Route element={<ProtectedRoute />}>
              <Route element={<AppLayout />}>
                <Route path="/videos/:id/edit" element={<VideoEditPage />} />
                <Route path="/videos/:id" element={<p>Detalle del video</p>} />
                <Route path="/videos" element={<p>Lista de videos</p>} />
              </Route>
            </Route>
            <Route path="/login" element={<LoginPage />} />
            <Route path="*" element={<p>Otra página</p>} />
          </Routes>
          <AuthReadyProbe />
          <LocationProbe />
          </QueryClientProvider>
        </AuthProvider>
      </MemoryRouter>
    </StrictMode>,
  );
  await act(async () => {
    resolveCurrentUser(restoredUser);
    await currentUserPromise;
  });
  await waitFor(() => expect(screen.getByTestId("auth-ready")).toHaveTextContent("ready"));
  return client;
}

function AuthReadyProbe(): JSX.Element {
  const { ready, user } = useAuth();
  return <output data-testid="auth-ready">{ready && user ? "ready" : "pending"}</output>;
}

function LocationProbe(): JSX.Element {
  const location = useLocation();
  const state = location.state as { message?: string } | null;
  const returnTo = new URLSearchParams(location.search).get("returnTo") ?? "";
  return <>
    <output data-testid="location">{location.pathname}</output>
    <output data-testid="location-message">{state?.message ?? ""}</output>
    <output data-testid="return-to">{returnTo}</output>
  </>;
}
