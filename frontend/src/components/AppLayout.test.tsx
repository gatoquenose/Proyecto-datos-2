import { render, screen } from "@testing-library/react";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { AppLayout } from "./AppLayout";
import { ProfilePage } from "../pages/ProfilePage";

const users = vi.hoisted(() => ({ getMyQuota: vi.fn() }));

vi.mock("../auth/AuthContext", () => ({
  useAuth: () => ({
    user: { id: "user-1", username: "ana.tec", email: "ana@example.test" },
    logout: vi.fn(),
  }),
}));

vi.mock("../api/usersClient", () => ({
  usersClient: {
    getMyQuota: users.getMyQuota,
    updateUser: vi.fn(),
    deleteUser: vi.fn(),
  },
}));

describe("AppLayout", () => {
  it("no muestra la cuota en la cabecera", () => {
    renderWithRoute("/videos", <p>Contenido</p>);
    expect(screen.queryByRole("region", { name: "Cuota de almacenamiento" })).not.toBeInTheDocument();
  });

  it("muestra la cuota únicamente en el perfil", async () => {
    users.getMyQuota.mockResolvedValueOnce({ quotaBytes: 1000, usedBytes: 250, remainingBytes: 750 });
    renderWithRoute("/profile", <ProfilePage />);
    expect(await screen.findByRole("region", { name: "Cuota de almacenamiento" })).toBeInTheDocument();
  });
});

function renderWithRoute(path: string, element: JSX.Element): void {
  const client = new QueryClient({ defaultOptions: { queries: { retry: false } } });
  render(
    <QueryClientProvider client={client}>
      <MemoryRouter initialEntries={[path]}>
        <Routes>
          <Route element={<AppLayout />}>
            <Route path="/videos" element={element} />
            <Route path="/profile" element={element} />
          </Route>
        </Routes>
      </MemoryRouter>
    </QueryClientProvider>,
  );
}
