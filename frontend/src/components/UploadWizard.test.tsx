import { act, fireEvent, render, screen, waitFor } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter, useLocation } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { UploadWizard, DEFAULT_MAX_UPLOAD_BYTES } from "./UploadWizard";
import type { UploadClients } from "../hooks/useVideoUpload";
import { ApiError } from "../types/api";

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
  render(<QueryClientProvider client={new QueryClient()}><MemoryRouter><UploadWizard username="ana.tec" clients={clients()} {...overrides} /><LocationProbe /></MemoryRouter></QueryClientProvider>);
}

function LocationProbe(): JSX.Element {
  return <div data-testid="upload-location">{useLocation().pathname}</div>;
}

describe("UploadWizard", () => {
  it.each(["", "no-numérico", "0", "-1", "1.5", "Infinity"])(
    "usa el máximo por defecto con VITE_MAX_UPLOAD_BYTES=%s",
    (configuredMaxUploadBytes) => {
      vi.stubEnv("VITE_MAX_UPLOAD_BYTES", configuredMaxUploadBytes);
      renderWizard();
      fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
      fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);

      fireEvent.change(screen.getByLabelText("Archivo de video"), {
        target: { files: [new File(["xx"], "pequeno.mp4", { type: "video/mp4" })] },
      });

      expect(screen.queryByRole("alert")).not.toBeInTheDocument();
      expect(screen.getByRole("button", { name: "Subir video" })).toBeEnabled();
    },
  );

  it("acepta VITE_MAX_UPLOAD_BYTES=1 sin imponer un mínimo práctico", () => {
    vi.stubEnv("VITE_MAX_UPLOAD_BYTES", "1");
    renderWizard();
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);

    fireEvent.change(screen.getByLabelText("Archivo de video"), {
      target: { files: [new File(["x"], "pequeno.mp4", { type: "video/mp4" })] },
    });

    expect(screen.queryByRole("alert")).not.toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Subir video" })).toBeEnabled();
  });

  it("valida metadatos, cambia de paso y conserva el foco en el encabezado", () => {
    renderWizard();
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    const title = screen.getByLabelText("Título");
    expect(title).toHaveAttribute("aria-invalid", "true");
    const titleErrorId = title.getAttribute("aria-describedby");
    expect(titleErrorId).toBe("video-title-error");
    expect(document.getElementById(titleErrorId!)).toHaveTextContent("El título es obligatorio");
    expect(screen.getByRole("alert")).toHaveTextContent("Corrige los errores del formulario.");
    expect(screen.getAllByRole("alert")).toHaveLength(1);
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

  it("muestra errores por campo, aria-describedby y enfoca el primer error", async () => {
    renderWizard();
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    const title = screen.getByLabelText("Título");
    expect(title).toHaveAttribute("aria-invalid", "true");
    expect(title).toHaveAttribute("aria-describedby", "video-title-error");
    await waitFor(() => expect(title).toHaveFocus());
    expect(screen.queryByText("El título es obligatorio", { selector: "label" })).not.toBeInTheDocument();
  });

  it("rechaza tipo y tamaño de archivo", () => {
    renderWizard();
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    const input = screen.getByLabelText("Archivo de video");
    fireEvent.change(input, { target: { files: [new File(["x"], "nota.txt", { type: "text/plain" })] } });
    expect(screen.getByRole("alert")).toHaveTextContent("Tipo no permitido");
    const large = new File(["x"], "grande.mp4", { type: "video/mp4" });
    Object.defineProperty(large, "size", { value: 2147483649 });
    fireEvent.change(input, { target: { files: [large] } });
    expect(screen.getByRole("alert")).toHaveTextContent("tamaño máximo");
  });

  it("ofrece reanudar el mismo archivo pendiente", () => {
    sessionStorage.setItem("tecflix.pending-upload", JSON.stringify({ uploadId: "u", videoId: "v", fileName: "clase.mp4", sizeBytes: 1, contentType: "video/mp4", chunkSize: 8 }));
    renderWizard();
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    fireEvent.change(screen.getByLabelText("Archivo de video"), { target: { files: [new File(["x"], "clase.mp4", { type: "video/mp4" })] } });
    expect(screen.getByRole("status")).toHaveTextContent("reanudarla");
  });

  it("muestra la barra de progreso con atributos y el anuncio de fase", async () => {
    let resolveChunk: (() => void) | undefined;
    const uploadClients = clients();
    uploadClients.createVideo = vi.fn().mockResolvedValue({ id: "video-1" });
    uploadClients.startUpload = vi.fn().mockResolvedValue({ uploadId: "upload-1", videoId: "video-1", reservedBytes: 1, receivedBytes: 0, status: "uploading" });
    uploadClients.uploadChunk = vi.fn().mockImplementation((_uploadId, _chunkNumber, _bytes, _checksum, _contentRange, options) =>
      new Promise<void>((resolve, reject) => {
        resolveChunk = resolve;
        options?.signal?.addEventListener("abort", () => reject(new DOMException("Aborted", "AbortError")), { once: true });
      }));
    uploadClients.completeUpload = vi.fn().mockResolvedValue({ status: "processing" });
    uploadClients.getProcessing = vi.fn().mockResolvedValue({ status: "ready" });
    renderWizard({ clients: uploadClients });
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    fireEvent.change(screen.getByLabelText("Archivo de video"), { target: { files: [new File(["x"], "clase.mp4", { type: "video/mp4" })] } });
    fireEvent.click(screen.getByRole("button", { name: "Subir video" }));
    const progress = await screen.findByRole("progressbar");
    expect(progress).toHaveAttribute("aria-valuemin", "0");
    expect(progress).toHaveAttribute("aria-valuemax", "100");
    expect(progress).toHaveAttribute("aria-valuenow", "0");
    expect(screen.getByText("Estado: subiendo")).toBeInTheDocument();
    const announcement = screen.getByText("Estado de la subida: subiendo");
    expect(announcement).toHaveAttribute("aria-live", "polite");
    await act(async () => { resolveChunk?.(); });
    await waitFor(() => expect(screen.getByTestId("upload-location")).toHaveTextContent("/videos/video-1"));
  });

  it("cancela una subida activa y mantiene beforeunload solo durante ella", async () => {
    const uploadClients = clients();
    uploadClients.createVideo = vi.fn().mockResolvedValue({ id: "video-1" });
    uploadClients.startUpload = vi.fn().mockResolvedValue({ uploadId: "upload-1", videoId: "video-1", reservedBytes: 1, receivedBytes: 0, status: "uploading" });
    uploadClients.uploadChunk = vi.fn().mockImplementation((_uploadId, _chunkNumber, _bytes, _checksum, _contentRange, options) =>
      new Promise<void>((_resolve, reject) => {
        const signal = options?.signal;
        const rejectAsAborted = (): void => reject(new DOMException("Aborted", "AbortError"));
        if (signal?.aborted) rejectAsAborted();
        else signal?.addEventListener("abort", rejectAsAborted, { once: true });
      }));
    uploadClients.cancelUpload = vi.fn().mockResolvedValue(undefined);
    uploadClients.deleteVideo = vi.fn().mockResolvedValue(undefined);
    const add = vi.spyOn(window, "addEventListener");
    const remove = vi.spyOn(window, "removeEventListener");
    renderWizard({ clients: uploadClients });
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    fireEvent.change(screen.getByLabelText("Archivo de video"), { target: { files: [new File(["x"], "clase.mp4", { type: "video/mp4" })] } });
    fireEvent.click(screen.getByRole("button", { name: "Subir video" }));
    await waitFor(() => expect(uploadClients.uploadChunk).toHaveBeenCalledTimes(1));
    expect(add).toHaveBeenCalledWith("beforeunload", expect.any(Function));
    fireEvent.click(screen.getByRole("button", { name: "Cancelar" }));
    await waitFor(() => expect(screen.getByText("Estado: cancelado")).toBeInTheDocument());
    expect(uploadClients.uploadChunk).toHaveBeenCalledTimes(1);
    expect(uploadClients.cancelUpload).toHaveBeenCalledWith("upload-1");
    expect(uploadClients.deleteVideo).toHaveBeenCalledWith("video-1");
    await waitFor(() => expect(remove).toHaveBeenCalledWith("beforeunload", expect.any(Function)));
  });

  it("ofrece reintentar y descartar cuando completeUpload falla", async () => {
    const uploadClients = clients();
    uploadClients.createVideo = vi.fn().mockResolvedValue({ id: "video-1" });
    uploadClients.startUpload = vi.fn().mockResolvedValue({ uploadId: "upload-1", videoId: "video-1", reservedBytes: 1, receivedBytes: 0, status: "uploading" });
    uploadClients.uploadChunk = vi.fn().mockResolvedValue(undefined);
    uploadClients.completeUpload = vi.fn().mockRejectedValueOnce(new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "trace" })).mockResolvedValue({ status: "processing" });
    uploadClients.getProcessing = vi.fn().mockResolvedValue({ status: "ready" });
    uploadClients.cancelUpload = vi.fn().mockResolvedValue(undefined);
    uploadClients.deleteVideo = vi.fn().mockResolvedValue(undefined);
    renderWizard({ clients: uploadClients });
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    fireEvent.change(screen.getByLabelText("Archivo de video"), { target: { files: [new File(["x"], "clase.mp4", { type: "video/mp4" })] } });
    fireEvent.click(screen.getByRole("button", { name: "Subir video" }));
    await screen.findByRole("button", { name: "Reintentar" });
    fireEvent.click(screen.getByRole("button", { name: "Reintentar" }));
    await waitFor(() => expect(uploadClients.startUpload).toHaveBeenCalledTimes(2));
    expect(screen.getByTestId("upload-location")).toHaveTextContent("/");
  });

  it("descarta una subida fallida y limpia sus registros", async () => {
    const uploadClients = clients();
    uploadClients.createVideo = vi.fn().mockResolvedValue({ id: "video-1" });
    uploadClients.startUpload = vi.fn().mockResolvedValue({ uploadId: "upload-1", videoId: "video-1", reservedBytes: 1, receivedBytes: 0, status: "uploading" });
    uploadClients.uploadChunk = vi.fn().mockResolvedValue(undefined);
    uploadClients.completeUpload = vi.fn().mockRejectedValue(new ApiError(400, { code: "VALIDATION_ERROR", message: "Checksum inválido", details: null, traceId: "trace" }));
    uploadClients.cancelUpload = vi.fn().mockResolvedValue(undefined);
    uploadClients.deleteVideo = vi.fn().mockResolvedValue(undefined);
    renderWizard({ clients: uploadClients });
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    fireEvent.change(screen.getByLabelText("Archivo de video"), { target: { files: [new File(["x"], "clase.mp4", { type: "video/mp4" })] } });
    fireEvent.click(screen.getByRole("button", { name: "Subir video" }));
    await screen.findByRole("button", { name: "Descartar" });
    fireEvent.click(screen.getByRole("button", { name: "Descartar" }));
    await waitFor(() => expect(uploadClients.deleteVideo).toHaveBeenCalledWith("video-1"));
  });

  it("navega al video publicado", async () => {
    const uploadClients = clients();
    uploadClients.createVideo = vi.fn().mockResolvedValue({ id: "video-1" });
    uploadClients.startUpload = vi.fn().mockResolvedValue({ uploadId: "upload-1", videoId: "video-1", reservedBytes: 1, receivedBytes: 0, status: "uploading" });
    uploadClients.uploadChunk = vi.fn().mockResolvedValue(undefined);
    uploadClients.completeUpload = vi.fn().mockResolvedValue({ status: "processing" });
    uploadClients.getProcessing = vi.fn().mockResolvedValue({ status: "ready" });
    renderWizard({ clients: uploadClients });
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Clase 1" } });
    fireEvent.submit(screen.getByRole("button", { name: "Continuar al archivo" }).closest("form")!);
    fireEvent.change(screen.getByLabelText("Archivo de video"), { target: { files: [new File(["x"], "clase.mp4", { type: "video/mp4" })] } });
    fireEvent.click(screen.getByRole("button", { name: "Subir video" }));
    await waitFor(() => expect(screen.getByTestId("upload-location")).toHaveTextContent("/videos/video-1"));
  });
});
