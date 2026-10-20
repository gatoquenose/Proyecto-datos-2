import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { LoginPage, RegisterPage } from "./LoginPage";
import { ApiError } from "../types/api";

const mocks = vi.hoisted(() => ({ login: vi.fn(), createUser: vi.fn() }));
vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ login: mocks.login }) }));
vi.mock("../api/usersClient", () => ({ usersClient: { login: mocks.login, createUser: mocks.createUser } }));

describe("F2 formularios", () => {
  it("muestra credenciales inválidas en el formulario sin aviso de sesión expirada", async () => {
    mocks.login.mockRejectedValueOnce(new ApiError(401, { code: "INVALID_CREDENTIALS", message: "Usuario o contraseña incorrectos.", details: null, traceId: "trace" }));
    render(<MemoryRouter><LoginPage /></MemoryRouter>);
    fireEvent.change(screen.getByLabelText("Usuario"), { target: { value: "ana.tec" } });
    fireEvent.change(screen.getByLabelText("Contraseña"), { target: { value: "incorrecta" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    await waitFor(() => expect(screen.getByRole("alert")).toHaveTextContent("Ocurrió un error inesperado."));
    expect(screen.queryByText("Tu sesión expiró.")).not.toBeInTheDocument();
  });
  it("mapea EMAIL_EXISTS al campo correo", async () => {
    mocks.createUser.mockRejectedValueOnce(new ApiError(409, { code: "EMAIL_EXISTS", message: "Correo existente.", details: null, traceId: "trace" }));
    render(<MemoryRouter><RegisterPage /></MemoryRouter>);
    fireEvent.change(screen.getByLabelText("Usuario"), { target: { value: "nuevo" } });
    fireEvent.change(screen.getByLabelText("Correo"), { target: { value: "nuevo@test.com" } });
    fireEvent.change(document.querySelector("#register-password") as HTMLInputElement, { target: { value: "password123" } });
    fireEvent.change(screen.getByLabelText("Confirmar contraseña"), { target: { value: "password123" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    await waitFor(() => expect(screen.getByText("Ese correo ya está registrado.")).toBeInTheDocument());
  });
});
