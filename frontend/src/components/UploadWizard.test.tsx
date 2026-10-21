import { fireEvent, render, screen } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { UploadWizard, DEFAULT_MAX_UPLOAD_BYTES } from "./UploadWizard";
import type { UploadClients } from "../hooks/useVideoUpload";

const clients = (): UploadClients => ({
  createVideo: vi.fn(),
  startUpload: vi.fn(),
  uploadChunk: vi.fn(),
  getUpload: vi.fn(),
  completeUpload: vi.fn(),
  getProcessing: vi.fn(),
  cancelUpload: vi.fn(),
  deleteVideo: vi.fn(),
});

function renderWizard(overrides: Partial<React.ComponentProps<typeof UploadWizard>> = {}): void {
  render(<QueryClientProvider client={new QueryClient()}><MemoryRouter><UploadWizard username="ana.tec" clients={clients()} {...overrides} /></MemoryRouter></QueryClientProvider>);
}

describe("UploadWizard", () => {
  it("valida metadatos, cambia de paso y conserva el foco en el encabezado", () => {
    renderWizard();
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    expect(screen.getByRole("alert")).toHaveTextContent("El título es obligatorio");
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    expect(screen.getByRole("heading", { name: "Archivo" })).toBeInTheDocument();
  });

  it("bloquea por cuota y explica la reserva adicional del servidor", () => {
    renderWizard({ remainingBytes: 1 });
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    const file = new File(["contenido"], "clase.mp4", { type: "video/mp4" });
    fireEvent.change(screen.getByLabelText("Archivo de video"), { target: { files: [file] } });
    expect(screen.getByRole("alert")).toHaveTextContent("reserva espacio adicional");
    expect(screen.getByRole("button", { name: "Subir video" })).toBeDisabled();
  });

  it("acepta los tipos del contrato y aplica el máximo por defecto", () => {
    expect(DEFAULT_MAX_UPLOAD_BYTES).toBe(2147483648);
    renderWizard();
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    expect(screen.getByLabelText("Archivo de video")).toHaveAttribute("accept", "video/mp4,video/webm,video/quicktime,video/x-matroska");
  });
});
