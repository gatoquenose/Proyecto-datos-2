import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Proxy de desarrollo: el navegador llama a /svc/... en el mismo origen
// y Vite reenvia a cada API, asi no hay peticiones cruzadas (CORS).
const proxyTo = (port, prefix) => ({
  target: `http://localhost:${port}`,
  changeOrigin: true,
  rewrite: (path) => path.replace(new RegExp(`^${prefix}`), "/api/v1"),
});

export default defineConfig({
  plugins: [react()],
  server: {
    proxy: {
      "/svc/users": proxyTo(8081, "/svc/users"),
      "/svc/metadata": proxyTo(8082, "/svc/metadata"),
      "/svc/streaming": proxyTo(8083, "/svc/streaming"),
    },
  },
  test: {
    environment: "jsdom",
    setupFiles: "./src/test/setup.ts",
    globals: true,
  },
});