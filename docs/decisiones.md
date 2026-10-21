# Decisiones aceptadas

- Upload verifies metadata ownership, published/status eligibility, and that no
  other active upload exists before allocation.
- Quota reservations are persistent records with expiry timestamps and a cleanup
  worker; reservations are released on failure/expiry.
- Deleting a published video releases its reserved quota.
- Usernames are immutable after account creation.
- The frontend fetches protected thumbnails with `Authorization` and uses Blob
  URLs. There is no refresh endpoint: expired tokens or any 401 clear the
  session and return to login while preserving the intended route.
- The effect of account deletion on the user's videos is still pending. The
  proposed backend behavior is to reject deletion with `409 INVALID_STATE` when
  the user owns videos; the profile dialog therefore makes no promise about
  what happens to videos.
- MongoDB indexes cover `title`, `tags`, and `topic`, use case-insensitive
  collation, and every index query is validated with `explain`.
- User endpoints return only privacy-scoped data and never expose credentials,
  private metadata, or internal identifiers unnecessarily.
- El fixture DASH sintético de mocks vive en
  `frontend/src/mocks/fixtures/dash/`. Se generó con FFmpeg mediante:
  `ffmpeg -y -f lavfi -i "testsrc=size=1280x720:rate=24:duration=12" -f lavfi -i "sine=frequency=1000:sample_rate=48000:duration=12" -filter_complex "[0:v]split=2[v360][v720];[v360]scale=640:360[v360out];[v720]null[v720out]" -map "[v360out]" -map "[v720out]" -map 1:a:0 -c:v libx264 -preset veryfast -tune zerolatency -pix_fmt yuv420p -r 24 -g 96 -keyint_min 96 -sc_threshold 0 -b:v:0 70k -maxrate:v:0 80k -bufsize:v:0 140k -b:v:1 150k -maxrate:v:1 170k -bufsize:v:1 300k -c:a aac -b:a 32k -ar 48000 -ac 1 -use_template 1 -use_timeline 1 -seg_duration 4 -adaptation_sets "id=0,streams=v id=1,streams=a" -init_seg_name 'init-$RepresentationID$.m4s' -media_seg_name 'seg-$RepresentationID$-$Number%05d$.m4s' -f dash "frontend\src\mocks\fixtures\dash\manifest.mpd"`.
  El manifiesto generado conserva nombres de segmento relativos; el handler
  MSW inyecta `<BaseURL>dash/</BaseURL>` al responder porque el manifiesto se
  sirve en `/videos/{id}/manifest.mpd` y los segmentos en
  `/videos/{id}/dash/{path}`. La especificación real debe resolver esta
  discrepancia antes de implementarse.
- Propuesta pendiente para `docs/openapi/streaming.yaml` (no aplicada):
  documentar que la respuesta de `GET /videos/{videoId}/manifest.mpd` incluye
  `BaseURL: dash/`, o alternativamente declarar explícitamente que el servidor
  reescribe las referencias relativas del MPD para resolver contra
  `/videos/{videoId}/dash/`.
- F4 carga Shaka de forma dinámica únicamente desde el reproductor de la
  página de detalle, instala sus polyfills y rechaza navegadores sin soporte.
  Las peticiones de manifiesto y segmentos llevan `Authorization`; el retry
  queda limitado a un intento para no reintentar 401, 403 ni 404. Los errores
  `BAD_HTTP_STATUS` se mapean a los mensajes de sesión expirada, permisos o
  recurso no encontrado; el 401 reutiliza el flujo global de sesión expirada.
- Con el backend real, el filtro de `Authorization` genera preflight CORS por
  segmento: Streaming API debe permitir el header `Authorization`, responder
  `OPTIONS` y exponer `Content-Range` y `Accept-Ranges`.
- F4 incluye acciones del dueño: Editar enlaza a la ruta de edición, Cambiar
  visibilidad usa `PATCH /videos/{id}/visibility` y Eliminar requiere
  confirmación y usa `DELETE /videos/{id}`. El diálogo no promete nada sobre
  los archivos.

## Frontend F1

- `openapi-typescript` is a dev dependency and `gen:api` generates TypeScript
  types directly from the three versioned OpenAPI contracts. This prevents
  hand-written client types from drifting from the source of truth.
- The JWT is kept in memory and mirrored in `sessionStorage`. This survives a
  tab reload but is cleared when the tab session ends; it remains exposed to
  JavaScript on the origin, so an XSS vulnerability could read it. There is no
  refresh endpoint; after reload the client validates it with `GET /users/me`.
- Docker Compose publishes Users, Metadata, and Streaming on host ports 8081,
  8082, and 8083 respectively. The frontend exposes matching `VITE_*_API_URL`
  variables, and real APIs must configure CORS for the frontend origin in
  phases C, D, and E.
- `react-router-dom`, TanStack Query, MSW, Shaka Player, `@noble/hashes`,
  Vitest, Testing Library, and jsdom support routing, server-state caching,
  contract mocks, DASH playback, SHA-256 fallback, and required tests.

## Propuesta de linting frontend

- `eslint` se propone como dependencia de desarrollo para detectar errores de
  calidad y consistencia antes del build.
- `eslint-plugin-react-hooks` se propone para validar las reglas de hooks,
  especialmente el orden estable de hooks que una prueba de F3 tuvo que
  detectar manualmente.
- `typescript-eslint` se propone para que ESLint analice el TypeScript del
  frontend con reglas tipadas y reduzca errores que `tsc` no identifica como
  problemas de estilo o uso de React.

Estas dependencias se instalaron después de aprobar la propuesta y quedaron
configuradas en `frontend/eslint.config.mjs`.

La propuesta fue aprobada y se instaló con Node `v22.16.0`: `eslint@9.39.5`
ejecuta el lint con la configuración flat; `typescript-eslint@8.72.0` aporta
parser y reglas para TypeScript; `eslint-plugin-react-hooks@7.1.1` detecta
violaciones de las reglas de hooks, incluido el orden estable que ya causó un
bug en F3; y `eslint-plugin-jsx-a11y@6.10.2` valida patrones de accesibilidad
en JSX. `eslint-plugin-jsx-a11y` se añadió porque las pantallas y controles
del frontend necesitan validación automatizada de labels, roles y atributos
ARIA. Todas son dependencias de desarrollo y se configuraron sin flags de
forzado.
