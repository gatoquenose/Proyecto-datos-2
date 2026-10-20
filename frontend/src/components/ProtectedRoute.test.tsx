import { render, screen } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { ProtectedRoute } from "./ProtectedRoute";

vi.mock("../auth/AuthContext", () => ({ useAuth: () => ({ token: null, ready: true }) }));

describe("ProtectedRoute", () => {
  it("redirige sin sesión", () => {
    render(<MemoryRouter initialEntries={["/videos"]}><Routes><Route element={<ProtectedRoute />}><Route path="/videos" element={<p>privado</p>} /></Route><Route path="/login" element={<p>login</p>} /></Routes></MemoryRouter>);
    expect(screen.getByText("login")).toBeInTheDocument();
  });
});
