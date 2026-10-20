import { fireEvent, render, screen } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { AuthProvider, useAuth } from "../auth/AuthContext";
import { LoginPage } from "../pages/LoginPage";
import { ProtectedRoute } from "./ProtectedRoute";
import { authStore } from "../auth/authStore";

const users = vi.hoisted(() => ({ getCurrentUser: vi.fn() }));
vi.mock("../api/usersClient", () => ({ usersClient: users }));

describe("aviso de sesión expirada", () => {
  beforeEach(() => {
    authStore.clear();
    users.getCurrentUser.mockReset();
  });

  it("no aparece al visitar una ruta protegida sin sesión", async () => {
    render(<MemoryRouter initialEntries={["/videos"]}><AuthProvider><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<p>privado</p>} /></Route><Route path="/login" element={<LoginPage />} /></Routes></AuthProvider></MemoryRouter>);
    await screen.findByRole("heading", { name: "Iniciar sesión" });
    expect(screen.queryByText("Tu sesión expiró.")).not.toBeInTheDocument();
  });

  it("no aparece después de cerrar sesión voluntariamente", async () => {
    authStore.setSession("jwt", { id: "1", username: "ana.tec", email: "ana@test.com", quotaBytes: 100, usedBytes: 25 });
    users.getCurrentUser.mockResolvedValueOnce({ id: "1", username: "ana.tec", email: "ana@test.com" });
    function ProtectedContent(): JSX.Element {
      const { logout } = useAuth();
      return <button onClick={logout}>Cerrar sesión</button>;
    }
    render(<MemoryRouter initialEntries={["/videos"]}><AuthProvider><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<ProtectedContent />} /></Route><Route path="/login" element={<LoginPage />} /></Routes></AuthProvider></MemoryRouter>);
    fireEvent.click(await screen.findByRole("button", { name: "Cerrar sesión" }));
    await screen.findByRole("heading", { name: "Iniciar sesión" });
    expect(screen.queryByText("Tu sesión expiró.")).not.toBeInTheDocument();
  });
});
