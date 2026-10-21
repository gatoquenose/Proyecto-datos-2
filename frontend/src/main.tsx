import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { BrowserRouter } from "react-router-dom";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { AuthProvider } from "./auth/AuthContext";
import { App } from "./app/App";
import "./style.css";

const queryClient = new QueryClient();
const root = createRoot(document.getElementById("root")!);
const render = (): void => root.render(<StrictMode><BrowserRouter><AuthProvider><QueryClientProvider client={queryClient}><App /></QueryClientProvider></AuthProvider></BrowserRouter></StrictMode>);

if (import.meta.env.DEV && import.meta.env.VITE_USE_MOCKS === "true") {
  void import("./mocks/browser").then(({ worker }) => worker.start({ onUnhandledRequest: "bypass" })).then(render);
} else {
  render();
}
