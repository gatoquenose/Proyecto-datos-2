# Tecflix Frontend

## 1. Qué es y estado
Tecflix Frontend es la aplicación web desarrollada en React 18, TypeScript y Vite para la gestión y reproducción de videos. Estado actual: Fases F0–F5 completadas y en proceso de pulido F6.

## 2. Requisitos
- Node 22
- En Windows PowerShell usar `npm.cmd`.

## 3. Instalación y comandos
- `npm.cmd install` — Instala las dependencias del proyecto.
- `npm.cmd run dev` — Inicia el servidor de desarrollo de Vite.
- `npm.cmd run build` — Compila la aplicación para producción.
- `npm.cmd run lint` — Ejecuta el linter ESLint.
- `npm.cmd run preview` — Previsualiza el build de producción.
- `npm.cmd run gen:api` — Genera los tipos TypeScript desde los contratos OpenAPI.
- `npm.cmd test` — Ejecuta las pruebas unitarias con Vitest.
- `npm.cmd run test:run` — Ejecuta la suite de pruebas una sola vez.

## 4. Variables de entorno
Configuradas en `.env.example`:
- `VITE_USERS_API_URL`: URL base de la API de usuarios.
- `VITE_METADATA_API_URL`: URL base de la API de metadata.
- `VITE_STREAMING_API_URL`: URL base de la API de streaming.
- `VITE_USE_MOCKS`: Activa o desactiva el uso de MSW para mocks (`true`/`false`).
- `VITE_UPLOAD_CHUNK_SIZE`: Tamaño de fragmento para subidas.
- `VITE_MOCK_UPLOAD_DELAY_MS`: Demora simulada en subidas mock.
- `VITE_MOCK_UPLOAD_FAILURE`: Configuración de fallos simulados de subida.
- `VITE_MOCK_QUOTA_REMAINING_BYTES`: Cuota restante simulada.
- `VITE_MAX_UPLOAD_BYTES`: Tamaño máximo permitido para subidas de archivos.

## 5. Modo con mocks
- Cómo activarlos: configurando `VITE_USE_MOCKS=true` en desarrollo.
- Usuario de prueba (según SPEC): `ana.tec` / `CorrectHorseBatteryStaple!` (email `ana@example.test`, ID `550e8400-e29b-41d4-a716-446655440000`, cuota 5368709120 bytes, usados 104857600 bytes).
- Funcionan únicamente con `npm.cmd --prefix frontend run dev`, no con `vite preview`.

## 6. Estructura de carpetas de `frontend/src`
- `api`
- `app`
- `auth`
- `components`
- `hooks`
- `mocks`
- `pages`
- `test`
- `types`
- `utils`

## 7. Pruebas
- Comando oficial: `npm.cmd --prefix frontend run test:run`.

## 8. Reglas del proyecto
- Los tipos se generan con `gen:api` y no se editan a mano.
- La cuota se muestra únicamente en `/profile`.
- El JWT se almacena en `sessionStorage`.
