import "@testing-library/jest-dom/vitest";
import { cleanup } from "@testing-library/react";
import { afterAll, afterEach, beforeAll, vi } from "vitest";
import { authStore } from "../auth/authStore";
import { server } from "../mocks/server";

beforeAll(() => server.listen({ onUnhandledRequest: "bypass" }));
afterEach(() => {
  cleanup();
  server.resetHandlers();
  authStore.clear();
  sessionStorage.clear();
  vi.restoreAllMocks();
  vi.clearAllMocks();
});
afterAll(() => server.close());
