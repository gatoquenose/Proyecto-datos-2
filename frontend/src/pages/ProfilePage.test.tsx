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
    authStore.setSession("jwt", { id: "1", username: "ana.tec", email: "ana@test.com", quotaBytes: 100, usedBytes: 25 });
    users.getCurrentUser.mockResolvedValue({ id: "1", username: "ana.tec", email: "ana@test.com" });
    users.getMyQuota.mockResolvedValue({ quotaBytes: 5368709120, usedBytes: 104857600, remainingBytes: 5263851520 });
    users.updateUser.mockReset();
    users.deleteUser.mockReset();
  });

  afterEach(() => {
    authStore.clear();
  });

  it("muestra cuota de solo lectura y username inmutable", async () => {
    renderProfile();
    expect(await screen.findByText("0.10 GB / 5.00 GB")).toBeInTheDocument();
    expect(screen.getByText("4.90 GB disponibles")).toBeInTheDocument();
    expect(screen.getByText("El username es inmutable.")).toBeInTheDocument();
    expect(screen.getByRole("region", { name: "Cuota de almacenamiento" })).toBeInTheDocument();
    expect(screen.queryByRole("textbox", { name: /cuota/i })).not.toBeInTheDocument();
    expect(screen.queryByRole("spinbutton", { name: /cuota/i })).not.toBeInTheDocument();
  });

  it("edita el correo enviando solo email y muestra errores por campo", async () => {
    users.updateUser.mockRejectedValueOnce(new ApiError(409, { code: "EMAIL_EXISTS", message: "Correo existente.", details: null, traceId: "trace" }));
    renderProfile();
    await screen.findByText("0.10 GB / 5.00 GB");
    const email = screen.getByLabelText("Correo");
    fireEvent.change(email, { target: { value: "new@test.com" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar correo" }));
    await waitFor(() => expect(users.updateUser).toHaveBeenCalledWith("1", { email: "new@test.com" }));
    await waitFor(() => expect(screen.getByText("Ese correo ya está registrado.")).toBeInTheDocument());
    expect(email).toHaveAttribute("aria-invalid", "true");
  });

  it.each(["Ana.Tec", "ana.tec "])("mantiene deshabilitada la eliminación para %j", async (value) => {
    renderProfile();
    await screen.findByText("0.10 GB / 5.00 GB");
    fireEvent.click(screen.getByRole("button", { name: "Eliminar mi cuenta" }));
    const confirm = screen.getByRole("button", { name: "Eliminar cuenta" });
    expect(confirm).toBeDisabled();
    fireEvent.change(screen.getByLabelText("Confirmar username"), { target: { value } });
    expect(confirm).toBeDisabled();
  });

  it("cierra con Escape y limpia el username al reabrir el diálogo", async () => {
    renderProfile();
    await screen.findByText("0.10 GB / 5.00 GB");
    fireEvent.click(screen.getByRole("button", { name: "Eliminar mi cuenta" }));
    fireEvent.change(screen.getByLabelText("Confirmar username"), { target: { value: "ana.tec" } });
    expect(screen.getByRole("button", { name: "Eliminar cuenta" })).toBeEnabled();
    fireEvent.keyDown(document, { key: "Escape" });
    expect(screen.queryByRole("dialog")).not.toBeInTheDocument();
    fireEvent.click(screen.getByRole("button", { name: "Eliminar mi cuenta" }));
    expect(screen.getByLabelText("Confirmar username")).toHaveValue("");
    expect(screen.getByRole("button", { name: "Eliminar cuenta" })).toBeDisabled();
  });

  it("elimina con éxito, limpia la sesión y navega al login", async () => {
    users.deleteUser.mockResolvedValueOnce(undefined);
    renderProfile();
    await screen.findByText("0.10 GB / 5.00 GB");
    fireEvent.click(screen.getByRole("button", { name: "Eliminar mi cuenta" }));
    fireEvent.change(screen.getByLabelText("Confirmar username"), { target: { value: "ana.tec" } });
    expect(screen.getByRole("button", { name: "Eliminar cuenta" })).toBeEnabled();
    fireEvent.click(screen.getByRole("button", { name: "Eliminar cuenta" }));
    await waitFor(() => expect(users.deleteUser).toHaveBeenCalledWith("1"));
    await waitFor(() => expect(screen.getByText("login")).toBeInTheDocument());
    expect(authStore.getToken()).toBeNull();
  });

  it("muestra el error de eliminación y conserva la sesión", async () => {
    users.deleteUser.mockRejectedValueOnce(new ApiError(409, { code: "INVALID_STATE", message: "No se puede eliminar.", details: null, traceId: "delete-trace" }));
    renderProfile();
    await screen.findByText("0.10 GB / 5.00 GB");
    fireEvent.click(screen.getByRole("button", { name: "Eliminar mi cuenta" }));
    fireEvent.change(screen.getByLabelText("Confirmar username"), { target: { value: "ana.tec" } });
    fireEvent.click(screen.getByRole("button", { name: "Eliminar cuenta" }));
    await waitFor(() => expect(screen.getByRole("status")).toHaveTextContent("La operación no es válida en este estado. (delete-trace)"));
    expect(authStore.getToken()).toBe("jwt");
    expect(screen.getByRole("heading", { name: "Mi perfil" })).toBeInTheDocument();
  });

  function renderProfile(): void {
    const client = new QueryClient();
    render(<QueryClientProvider client={client}><MemoryRouter initialEntries={["/profile"]}><AuthProvider><Routes><Route path="/profile" element={<ProfilePage />} /><Route path="/login" element={<p>login</p>} /></Routes></AuthProvider></MemoryRouter></QueryClientProvider>);
  }
});
