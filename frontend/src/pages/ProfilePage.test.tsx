import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { ProfilePage } from "./ProfilePage";
import { AuthProvider } from "../auth/AuthContext";
import { authStore } from "../auth/authStore";
import { ApiError } from "../types/api";

const users = vi.hoisted(() => ({ getCurrentUser: vi.fn(), getMyQuota: vi.fn(), updateUser: vi.fn(), deleteUser: vi.fn() }));
vi.mock("../api/usersClient", () => ({ usersClient: users }));

describe("ProfilePage", () => {
  beforeEach(() => {
    authStore.setSession("jwt", { id: "1", username: "ana.tec", email: "ana@test.com" });
    users.getCurrentUser.mockResolvedValue({ id: "1", username: "ana.tec", email: "ana@test.com" });
    users.getMyQuota.mockResolvedValue({ quotaBytes: 100, usedBytes: 25, remainingBytes: 75 });
    users.updateUser.mockReset();
    users.deleteUser.mockReset();
  });

  afterEach(() => {
    authStore.clear();
  });

  it("muestra cuota de solo lectura y username inmutable", async () => {
    renderProfile();
    expect(await screen.findByText("0.00 GB disponibles")).toBeInTheDocument();
    expect(screen.getByText("El username es inmutable.")).toBeInTheDocument();
    expect(screen.queryByLabelText(/cuota/i)).toBeInTheDocument();
  });

  it("edita el correo enviando solo email y muestra errores por campo", async () => {
    users.updateUser.mockRejectedValueOnce(new ApiError(409, { code: "EMAIL_EXISTS", message: "Correo existente.", details: null, traceId: "trace" }));
    renderProfile();
    await screen.findByText("0.00 GB disponibles");
    const email = screen.getByLabelText("Correo");
    fireEvent.change(email, { target: { value: "new@test.com" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar correo" }));
    await waitFor(() => expect(users.updateUser).toHaveBeenCalledWith("1", { email: "new@test.com" }));
    await waitFor(() => expect(screen.getByText("Ese correo ya está registrado.")).toBeInTheDocument());
    expect(email).toHaveAttribute("aria-invalid", "true");
  });

  it("exige el username exacto, cierra con Escape y redirige al login al eliminar", async () => {
    users.deleteUser.mockResolvedValueOnce(undefined);
    renderProfile();
    await screen.findByText("0.00 GB disponibles");
    fireEvent.click(screen.getByRole("button", { name: "Eliminar mi cuenta" }));
    const confirm = screen.getByRole("button", { name: "Eliminar cuenta" });
    expect(confirm).toBeDisabled();
    fireEvent.change(screen.getByLabelText("Confirmar username"), { target: { value: "incorrecto" } });
    expect(confirm).toBeDisabled();
    fireEvent.keyDown(document, { key: "Escape" });
    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "Eliminar mi cuenta" }));
    fireEvent.change(screen.getByLabelText("Confirmar username"), { target: { value: "ana.tec" } });
    expect(screen.getByRole("button", { name: "Eliminar cuenta" })).toBeEnabled();
    fireEvent.click(screen.getByRole("button", { name: "Eliminar cuenta" }));
    await waitFor(() => expect(users.deleteUser).toHaveBeenCalledWith("1"));
    await waitFor(() => expect(screen.getByText("login")).toBeInTheDocument());
  });

  function renderProfile(): void {
    const client = new QueryClient();
    render(<QueryClientProvider client={client}><MemoryRouter initialEntries={["/profile"]}><AuthProvider><Routes><Route path="/profile" element={<ProfilePage />} /><Route path="/login" element={<p>login</p>} /></Routes></AuthProvider></MemoryRouter></QueryClientProvider>);
  }
});
