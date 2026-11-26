import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { LoginPage, RegisterPage } from "./LoginPage";
import { ApiError } from "../types/api";

const mocks = vi.hoisted(() => ({ login: vi.fn(), createUser: vi.fn() }));
vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ login: mocks.login, sessionExpired: false }) }));
vi.mock("../api/usersClient", () => ({ usersClient: { login: mocks.login, createUser: mocks.createUser } }));

describe("F2 formularios", () => {
  it("valida todos los campos de registro por separado", async () => {
    render(<MemoryRouter><RegisterPage /></MemoryRouter>);
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    expect(screen.getByText("El usuario es obligatorio.")).toBeInTheDocument();
    expect(screen.getByText("El correo es obligatorio.")).toBeInTheDocument();
    expect(screen.getByText("La contraseña es obligatoria.")).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText("Contraseña"), { target: { value: "password123" } });
    fireEvent.change(screen.getByLabelText("Confirmar contraseña"), { target: { value: "different123" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    expect(screen.getByText("Las contraseñas no coinciden.")).toBeInTheDocument();
    expect(screen.getByLabelText("Usuario")).toHaveAttribute("aria-invalid", "true");
    expect(screen.getByLabelText("Correo")).toHaveAttribute("aria-invalid", "true");
    expect(screen.getByLabelText("Contraseña")).toHaveAttribute("aria-invalid", "false");
    expect(screen.getByLabelText("Confirmar contraseña")).toHaveAttribute("aria-invalid", "true");
    expect(mocks.createUser).not.toHaveBeenCalled();
  });

  it("mapea USERNAME_EXISTS al campo username", async () => {
    mocks.createUser.mockRejectedValueOnce(new ApiError(409, { code: "USERNAME_EXISTS", message: "Usuario existente.", details: null, traceId: "trace" }));
    render(<MemoryRouter><RegisterPage /></MemoryRouter>);
    fireEvent.change(screen.getByLabelText("Usuario"), { target: { value: "nuevo" } });
    fireEvent.change(screen.getByLabelText("Correo"), { target: { value: "nuevo@test.com" } });
    fireEvent.change(screen.getByLabelText("Contraseña"), { target: { value: "password123" } });
    fireEvent.change(screen.getByLabelText("Confirmar contraseña"), { target: { value: "password123" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    await waitFor(() => expect(screen.getByText("Ese nombre de usuario ya existe.")).toBeInTheDocument());
    expect(screen.getByLabelText("Usuario")).toHaveAttribute("aria-invalid", "true");
  });

  it("guarda la sesión y navega a returnTo tras un login exitoso", async () => {
    mocks.login.mockResolvedValueOnce({ accessToken: "jwt", user: { id: "1", username: "ana.tec", email: "ana@test.com" } });
    render(<MemoryRouter initialEntries={["/login?returnTo=%2Fvideos%2Fmine"]}><LoginPage /></MemoryRouter>);
    fireEvent.change(screen.getByLabelText("Usuario"), { target: { value: "ana.tec" } });
    fireEvent.change(screen.getByLabelText("Contraseña"), { target: { value: "password123" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    await waitFor(() => expect(mocks.login).toHaveBeenCalledWith("jwt", { id: "1", username: "ana.tec", email: "ana@test.com" }));
  });

  it("muestra credenciales inválidas en el formulario sin aviso de sesión expirada", async () => {
    mocks.login.mockRejectedValueOnce(new ApiError(401, { code: "INVALID_CREDENTIALS", message: "Usuario o contraseña incorrectos.", details: null, traceId: "trace" }));
    render(<MemoryRouter><LoginPage /></MemoryRouter>);
    fireEvent.change(screen.getByLabelText("Usuario"), { target: { value: "ana.tec" } });
    fireEvent.change(screen.getByLabelText("Contraseña"), { target: { value: "incorrecta" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    await waitFor(() => expect(screen.getByRole("alert")).toHaveTextContent("Usuario o contraseña incorrectos."));
    expect(screen.queryByText("Tu sesión expiró.")).not.toBeInTheDocument();
  });
  it("mapea EMAIL_EXISTS al campo correo", async () => {
    mocks.createUser.mockRejectedValueOnce(new ApiError(409, { code: "EMAIL_EXISTS", message: "Correo existente.", details: null, traceId: "trace" }));
    render(<MemoryRouter><RegisterPage /></MemoryRouter>);
    fireEvent.change(screen.getByLabelText("Usuario"), { target: { value: "nuevo" } });
    fireEvent.change(screen.getByLabelText("Correo"), { target: { value: "nuevo@test.com" } });
    fireEvent.change(screen.getByLabelText("Contraseña"), { target: { value: "password123" } });
    fireEvent.change(screen.getByLabelText("Confirmar contraseña"), { target: { value: "password123" } });
    fireEvent.click(screen.getByRole("button", { name: "Continuar" }));
    await waitFor(() => expect(screen.getByText("Ese correo ya está registrado.")).toBeInTheDocument());
  });
});
