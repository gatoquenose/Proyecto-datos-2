import { act, cleanup, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter, Route, Routes, useLocation } from "react-router-dom";
import { describe, expect, it, vi, beforeEach, afterEach } from "vitest";
import { ProtectedRoute } from "./ProtectedRoute";
import { AuthProvider, useAuth } from "../auth/AuthContext";
import { authStore } from "../auth/authStore";
import { request } from "../api/httpClient";
import { LoginPage } from "../pages/LoginPage";

const users = vi.hoisted(() => ({ getCurrentUser: vi.fn(), login: vi.fn() }));
vi.mock("../api/usersClient", () => ({ usersClient: users }));

describe("ProtectedRoute", () => {
  beforeEach(() => {
    authStore.clear();
    users.getCurrentUser.mockReset();
    users.login.mockReset();
  });

  afterEach(() => {
    cleanup();
    vi.restoreAllMocks();
    authStore.clear();
  });

  it("redirige sin sesión", () => {
    render(<MemoryRouter initialEntries={["/videos"]}><AuthProvider><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<p>privado</p>} /></Route><Route path="/login" element={<p>login</p>} /></Routes></AuthProvider></MemoryRouter>);
    expect(screen.getByText("login")).toBeInTheDocument();
  });

  it("con sesión muestra el contenido protegido", async () => {
    authStore.setSession("jwt", { id: "1", username: "ana.tec", email: "ana@test.com", quotaBytes: 100, usedBytes: 25 });
    const restoredUser = { id: "1", username: "ana.tec", email: "ana@test.com" };
    let resolveCurrentUser!: (user: typeof restoredUser) => void;
    const currentUserPromise = new Promise<typeof restoredUser>((resolve) => {
      resolveCurrentUser = resolve;
    });
    users.getCurrentUser.mockReturnValueOnce(currentUserPromise);
    render(<MemoryRouter initialEntries={["/videos"]}><AuthProvider><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<p>privado</p>} /></Route></Routes></AuthProvider></MemoryRouter>);
    await act(async () => {
      resolveCurrentUser(restoredUser);
      await currentUserPromise;
    });
    await waitFor(() => expect(screen.getByText("privado")).toBeInTheDocument());
  });

  it("restaura una sesión válida consultando GET /users/me", async () => {
    authStore.setSession("jwt", { id: "1", username: "ana.tec", email: "old@test.com", quotaBytes: 100, usedBytes: 25 });
    const restoredUser = { id: "1", username: "ana.tec", email: "restored@test.com" };
    let resolveCurrentUser!: (user: typeof restoredUser) => void;
    const currentUserPromise = new Promise<typeof restoredUser>((resolve) => {
      resolveCurrentUser = resolve;
    });
    users.getCurrentUser.mockReturnValueOnce(currentUserPromise);
    function RestoredUser(): JSX.Element {
      const { user } = useAuth();
      return <p>{user?.email}</p>;
    }
    render(<MemoryRouter initialEntries={["/videos"]}><AuthProvider><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<RestoredUser />} /></Route><Route path="/login" element={<p>login</p>} /></Routes></AuthProvider></MemoryRouter>);
    await act(async () => {
      resolveCurrentUser(restoredUser);
      await currentUserPromise;
    });
    await waitFor(() => expect(users.getCurrentUser).toHaveBeenCalled());
    await waitFor(() => expect(screen.getByText("restored@test.com")).toBeInTheDocument());
  });

  it("limpia una sesión inválida y redirige a login al restaurarla", async () => {
    authStore.setSession("jwt", { id: "1", username: "ana.tec", email: "old@test.com", quotaBytes: 100, usedBytes: 25 });
    let rejectCurrentUser!: (error: Error) => void;
    const currentUserPromise = new Promise<never>((_resolve, reject) => {
      rejectCurrentUser = reject;
    });
    users.getCurrentUser.mockReturnValueOnce(currentUserPromise);
    render(<MemoryRouter initialEntries={["/videos"]}><AuthProvider><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<p>privado</p>} /></Route><Route path="/login" element={<p>login</p>} /></Routes></AuthProvider></MemoryRouter>);
    await act(async () => {
      rejectCurrentUser(new Error("expired"));
      await currentUserPromise.catch(() => undefined);
    });
    await waitFor(() => expect(screen.getByText("login")).toBeInTheDocument());
    expect(authStore.getToken()).toBeNull();
  });


  it("avisa el 401, conserva la ruta y permite volver tras iniciar sesión", async () => {
    authStore.setSession("jwt", { id: "1", username: "ana.tec", email: "ana@test.com", quotaBytes: 100, usedBytes: 25 });
    const restoredUser = { id: "1", username: "ana.tec", email: "ana@test.com" };
    let resolveCurrentUser!: (user: typeof restoredUser) => void;
    const currentUserPromise = new Promise<typeof restoredUser>((resolve) => {
      resolveCurrentUser = resolve;
    });
    users.getCurrentUser.mockReturnValueOnce(currentUserPromise);
    users.login.mockResolvedValueOnce({ accessToken: "new-jwt", user: { id: "1", username: "ana.tec", email: "ana@test.com" } });
    vi.spyOn(globalThis, "fetch").mockResolvedValueOnce(new Response(JSON.stringify({ code: "AUTH_REQUIRED", message: "expired", details: null, traceId: "trace" }), { status: 401, headers: { "Content-Type": "application/json" } }));
    function ExpiringPage(): JSX.Element {
      const location = useLocation();
      return <><p>privado</p><p>{location.pathname}</p><button onClick={() => void request("/protected").catch(() => undefined)}>Actualizar</button></>;
    }
    render(<MemoryRouter initialEntries={["/videos"]}><AuthProvider><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<ExpiringPage />} /></Route><Route path="/login" element={<LoginPage />} /></Routes></AuthProvider></MemoryRouter>);
    await act(async () => {
      resolveCurrentUser(restoredUser);
      await currentUserPromise;
    });
    fireEvent.click(await screen.findByRole("button", { name: "Actualizar" }));
    await screen.findByRole("heading", { name: "Iniciar sesión" });
    await waitFor(() => expect(screen.getByRole("alert")).toHaveTextContent("Tu sesión expiró."));
    expect(authStore.getToken()).toBeNull();
    fireEvent.change(screen.getByLabelText("Usuario"), { target: { value: "ana.tec" } });
    fireEvent.change(screen.getByLabelText("Contraseña"), { target: { value: "password123" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    await waitFor(() => expect(screen.getByText("privado")).toBeInTheDocument());
    expect(screen.getByText("/videos")).toBeInTheDocument();
  });
});
