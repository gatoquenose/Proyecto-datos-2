import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter, Route, Routes, useLocation } from "react-router-dom";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { ApiError } from "../types/api";
import { VideoEditForm } from "./VideoEditForm";

const api = vi.hoisted(() => ({
  getVideo: vi.fn(),
  updateVideo: vi.fn(),
  updateVideoVisibility: vi.fn(),
}));

vi.mock("../api/metadataClient", () => ({ metadataClient: api }));

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
  api.getVideo.mockReset().mockResolvedValue(video);
  api.updateVideo.mockReset().mockResolvedValue(video);
  api.updateVideoVisibility.mockReset().mockResolvedValue(video);
});

afterEach(() => {
  vi.restoreAllMocks();
  vi.unstubAllEnvs();
});

describe("VideoEditForm", () => {
  it("carga los campos editables y muestra los datos no editables como solo lectura", async () => {
    renderForm();

    expect(await screen.findByLabelText("Título")).toHaveValue("Clase");
    expect(screen.getByLabelText("Descripción")).toHaveValue("Descripción");
    expect(screen.getByLabelText("Autor")).toHaveValue("Ana");
    expect(screen.getByLabelText("Tema")).toHaveValue("Algoritmos");
    expect(screen.getByLabelText("Etiquetas")).toHaveValue("tag");
    expect(screen.getByLabelText("Fecha y hora de carga")).toHaveValue(toLocalDateTimeValue(video.uploadedAt));
    expect(screen.getByText("Duración: 2:05")).toBeInTheDocument();
    expect(screen.getByText("Tamaño: 1.00 MB")).toBeInTheDocument();
    expect(screen.getByText("Estado: Listo")).toBeInTheDocument();
    expect(screen.queryByLabelText(/owner/i)).not.toBeInTheDocument();
    expect(screen.queryByDisplayValue("owner-1")).not.toBeInTheDocument();
    expect(screen.queryByDisplayValue("owner")).not.toBeInTheDocument();
  });

  it("acepta los límites exactos definidos en el contrato", async () => {
    const client = renderForm();
    await screen.findByLabelText("Título");
    const title = "T".repeat(200);
    const description = "D".repeat(5000);
    const author = "A".repeat(200);
    const topic = "T".repeat(100);
    const tags = Array.from({ length: 30 }, () => "x".repeat(50));

    fireEvent.change(screen.getByLabelText("Título"), { target: { value: title } });
    fireEvent.change(screen.getByLabelText("Descripción"), { target: { value: description } });
    fireEvent.change(screen.getByLabelText("Autor"), { target: { value: author } });
    fireEvent.change(screen.getByLabelText("Tema"), { target: { value: topic } });
    fireEvent.change(screen.getByLabelText("Etiquetas"), { target: { value: tags.join(",") } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await waitFor(() => expect(api.updateVideo).toHaveBeenCalledWith("video-1", {
      title,
      description,
      author,
      topic,
      tags,
    }));
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["videos"] });
    expect(client.invalidateQueries).not.toHaveBeenCalledWith({ queryKey: ["quota"] });
  });

  it.each([
    ["título vacío", "Título", "", "title", "El título es obligatorio y debe tener entre 1 y 200 caracteres."],
    ["descripción sobre 5000 caracteres", "Descripción", "D".repeat(5001), "description", "La descripción no puede superar 5000 caracteres."],
    ["autor sobre 200 caracteres", "Autor", "A".repeat(201), "author", "El autor no puede superar 200 caracteres."],
    ["tema sobre 100 caracteres", "Tema", "T".repeat(101), "topic", "El tema no puede superar 100 caracteres."],
    ["31 etiquetas", "Etiquetas", Array.from({ length: 31 }, () => "tag").join(","), "tags", "Las etiquetas deben ser hasta 30 y tener máximo 50 caracteres."],
    ["etiqueta de 51 caracteres", "Etiquetas", "x".repeat(51), "tags", "Las etiquetas deben ser hasta 30 y tener máximo 50 caracteres."],
  ])("rechaza %s sin enviar PATCH y asocia el error al campo", async (_case, label, value, field, message) => {
    renderForm();
    const input = await screen.findByLabelText(label);
    fireEvent.change(input, { target: { value } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    const error = await screen.findByText(message);
    expect(api.updateVideo).not.toHaveBeenCalled();
    expect(api.updateVideoVisibility).not.toHaveBeenCalled();
    expect(input).toHaveAttribute("aria-invalid", "true");
    const errorId = `video-edit-${field}-error`;
    expect(input.getAttribute("aria-describedby")?.split(" ")).toContain(errorId);
    expect(error.closest("label")).toBeNull();
    expect(input.closest("label")).not.toContainElement(error);
  });

  it("enfoca el primer campo inválido después de validar el formulario", async () => {
    renderForm();
    const title = await screen.findByLabelText("Título");
    fireEvent.change(title, { target: { value: "" } });
    fireEvent.change(screen.getByLabelText("Descripción"), { target: { value: "D".repeat(5001) } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await waitFor(() => expect(title).toHaveFocus());
    expect(api.updateVideo).not.toHaveBeenCalled();
  });

  it("no envía un PATCH vacío ni invalida cachés cuando no hay cambios", async () => {
    const client = renderForm();
    await screen.findByLabelText("Título");
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    expect(await screen.findByText("No hay cambios para guardar.")).toBeInTheDocument();
    expect(api.updateVideo).not.toHaveBeenCalled();
    expect(api.updateVideoVisibility).not.toHaveBeenCalled();
    expect(client.invalidateQueries).not.toHaveBeenCalled();
  });

  it("envía solo los campos cambiados y no incluye datos de solo lectura", async () => {
    const client = renderForm();
    await screen.findByLabelText("Título");
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Nueva clase" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await waitFor(() => expect(api.updateVideo).toHaveBeenCalledWith("video-1", { title: "Nueva clase" }));
    expect(api.updateVideo.mock.calls[0][1]).not.toHaveProperty("id");
    expect(api.updateVideo.mock.calls[0][1]).not.toHaveProperty("ownerId");
    expect(api.updateVideo.mock.calls[0][1]).not.toHaveProperty("ownerUsername");
    expect(api.updateVideo.mock.calls[0][1]).not.toHaveProperty("durationSeconds");
    expect(api.updateVideo.mock.calls[0][1]).not.toHaveProperty("sizeBytes");
    expect(api.updateVideo.mock.calls[0][1]).not.toHaveProperty("status");
    await waitFor(() => expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["videos"] }));
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
    expect(client.invalidateQueries).not.toHaveBeenCalledWith({ queryKey: ["quota"] });
  });

  it.each([
    ["UTC", "2026-10-19T17:00", "2026-10-19T17:00:00.000Z"],
    ["America/Costa_Rica", "2026-10-19T17:00", "2026-10-19T23:00:00.000Z"],
  ])("convierte uploadedAt local a UTC en la zona %s", async (timeZone, localValue, expectedUtc) => {
    vi.stubEnv("TZ", timeZone);
    renderForm();
    await screen.findByLabelText("Fecha y hora de carga");
    fireEvent.change(screen.getByLabelText("Fecha y hora de carga"), {
      target: { value: localValue },
    });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await waitFor(() => expect(api.updateVideo).toHaveBeenCalledWith("video-1", { uploadedAt: expectedUtc }));
  });

  it("cambia visibilidad sin actualizar metadata, invalida video y lista y permanece en edición", async () => {
    const client = renderForm();
    await screen.findByLabelText("Acceso al video");
    fireEvent.change(screen.getByLabelText("Acceso al video"), { target: { value: "public" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    await waitFor(() => expect(api.updateVideoVisibility).toHaveBeenCalledWith("video-1", "public"));
    expect(api.updateVideo).not.toHaveBeenCalled();
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["video", "video-1"] });
    expect(client.invalidateQueries).toHaveBeenCalledWith({ queryKey: ["videos"] });
    expect(client.invalidateQueries).not.toHaveBeenCalledWith({ queryKey: ["quota"] });
    expect(screen.getByTestId("location")).toHaveTextContent("/videos/video-1/edit");
  });

  it("muestra el error del servidor al fallar el cambio de visibilidad", async () => {
    api.updateVideoVisibility.mockRejectedValue(new ApiError(503, {
      code: "SERVICE_UNAVAILABLE",
      message: "No se pudo cambiar la visibilidad.",
      details: null,
      traceId: "test",
    }));
    renderForm();
    await screen.findByLabelText("Acceso al video");
    fireEvent.change(screen.getByLabelText("Acceso al video"), { target: { value: "public" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    expect(await screen.findByRole("alert")).toHaveTextContent("No se pudo cambiar la visibilidad.");
    expect(api.updateVideo).not.toHaveBeenCalled();
  });

  it("muestra el mensaje del servidor ante un error 5xx al guardar metadata", async () => {
    api.updateVideo.mockRejectedValue(new ApiError(503, {
      code: "SERVICE_UNAVAILABLE",
      message: "Metadata no está disponible.",
      details: null,
      traceId: "test",
    }));
    renderForm();
    await screen.findByLabelText("Título");
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Nueva clase" } });
    fireEvent.click(screen.getByRole("button", { name: "Guardar" }));

    expect(await screen.findByRole("alert")).toHaveTextContent("Metadata no está disponible.");
  });

  it("bloquea un segundo envío mientras la primera mutation sigue pendiente", async () => {
    let resolveUpdate: ((value: typeof video) => void) | undefined;
    api.updateVideo.mockImplementation(() => new Promise((resolve) => { resolveUpdate = resolve; }));
    const user = userEvent.setup();
    renderForm();
    await screen.findByLabelText("Título");
    fireEvent.change(screen.getByLabelText("Título"), { target: { value: "Nueva clase" } });
    const save = screen.getByRole("button", { name: "Guardar" });
    await user.click(save);

    await waitFor(() => expect(api.updateVideo).toHaveBeenCalledTimes(1));
    expect(screen.getByRole("button", { name: "Guardando…" })).toBeDisabled();
    await user.click(screen.getByRole("button", { name: "Guardando…" }));
    expect(api.updateVideo).toHaveBeenCalledTimes(1);
    resolveUpdate?.(video);
    expect(await screen.findByText("Metadatos actualizados.")).toBeInTheDocument();
  });

  it("permite recorrer Volver y Guardar con Tab y Volver cancela sin guardar ni invalidar", async () => {
    const client = renderForm();
    const user = userEvent.setup();
    await screen.findByLabelText("Título");
    const order = [
      screen.getByLabelText("Título"),
      screen.getByLabelText("Descripción"),
      screen.getByLabelText("Autor"),
      screen.getByLabelText("Tema"),
      screen.getByLabelText("Etiquetas"),
      screen.getByLabelText("Fecha y hora de carga"),
      screen.getByLabelText("Acceso al video"),
      screen.getByRole("link", { name: "Volver" }),
      screen.getByRole("button", { name: "Guardar" }),
    ];

    for (const element of order) {
      await user.tab();
      expect(element).toHaveFocus();
    }
    await user.tab({ shift: true });
    expect(screen.getByRole("link", { name: "Volver" })).toHaveFocus();
    await user.keyboard("{Enter}");

    expect(screen.getByTestId("location")).toHaveTextContent("/videos/video-1");
    expect(api.updateVideo).not.toHaveBeenCalled();
    expect(api.updateVideoVisibility).not.toHaveBeenCalled();
    expect(client.invalidateQueries).not.toHaveBeenCalled();
  });
});

function renderForm(): QueryClient {
  const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  vi.spyOn(client, "invalidateQueries");
  render(
    <QueryClientProvider client={client}>
      <MemoryRouter initialEntries={["/videos/video-1/edit"]}>
        <Routes>
          <Route path="/videos/:id/edit" element={
            <VideoEditForm
              videoId="video-1"
              ownerId="owner-1"
              currentUserId="owner-1"
              onSuccess={vi.fn()}
              onError={vi.fn()}
            />
          } />
          <Route path="/videos/:id" element={<p>Detalle del video</p>} />
        </Routes>
        <LocationProbe />
      </MemoryRouter>
    </QueryClientProvider>,
  );
  return client;
}

function toLocalDateTimeValue(value: string): string {
  const date = new Date(value);
  const pad = (part: number): string => String(part).padStart(2, "0");
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}T${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

function LocationProbe(): JSX.Element {
  const location = useLocation();
  return <output data-testid="location">{location.pathname}</output>;
}
