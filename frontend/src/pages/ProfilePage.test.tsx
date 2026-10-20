import { render, screen } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { ProfilePage } from "./ProfilePage";

vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ user: { id: "1", username: "ana.tec", email: "ana@test.com" }, logout: vi.fn() }) }));
vi.mock("../api/usersClient", () => ({ usersClient: { getMyQuota: vi.fn().mockResolvedValue({ quotaBytes: 100, usedBytes: 25, remainingBytes: 75 }), updateUser: vi.fn(), deleteUser: vi.fn() } }));

describe("ProfilePage", () => {
  it("muestra cuota de solo lectura y username inmutable", async () => {
    const client = new QueryClient();
    render(<QueryClientProvider client={client}><MemoryRouter><ProfilePage /></MemoryRouter></QueryClientProvider>);
    expect(await screen.findByText("0.00 GB disponibles")).toBeInTheDocument();
    expect(screen.getByText("El username es inmutable.")).toBeInTheDocument();
    expect(screen.queryByLabelText(/cuota/i)).toBeInTheDocument();
  });
});
