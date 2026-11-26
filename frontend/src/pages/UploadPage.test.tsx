import { render, screen } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { UploadPage } from "./UploadPage";

vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ user: { username: "ana.tec" } }) }));
vi.mock("../api/usersClient", () => ({ usersClient: { getMyQuota: vi.fn().mockResolvedValue({ quotaBytes: 100, usedBytes: 100, remainingBytes: 0 }) } }));
vi.mock("../api/metadataClient", () => ({ metadataClient: { createVideo: vi.fn(), deleteVideo: vi.fn() } }));
vi.mock("../api/streamingClient", () => ({ streamingClient: { startUpload: vi.fn(), uploadChunk: vi.fn(), getUpload: vi.fn(), completeUpload: vi.fn(), getProcessing: vi.fn(), cancelUpload: vi.fn() } }));

describe("UploadPage", () => {
  it("muestra el asistente y reserva la explicación de cuota para esta ruta", async () => {
    render(<QueryClientProvider client={new QueryClient()}><MemoryRouter><UploadPage /></MemoryRouter></QueryClientProvider>);
    expect(screen.getByRole("heading", { name: "Subir video" })).toBeInTheDocument();
    expect(await screen.findByText("Paso 1 de 2: metadatos")).toBeInTheDocument();
  });
});
