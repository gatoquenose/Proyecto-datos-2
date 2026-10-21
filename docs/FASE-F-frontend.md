# Fase F: Aplicación web de Tecflix (React + TypeScript)

Fuente de verdad: SPEC.md y los contratos docs/openapi/users.yaml, metadata.yaml y streaming.yaml. Si este documento contradice un YAML, manda el YAML. No modifiques los backends; si necesitas un cambio de contrato, propónmelo sin aplicarlo.

## Stack y reglas
- React 18 + TypeScript en modo strict (sin any) + Vite + React Router.
- Tipos generados desde los YAML con openapi-typescript (script npm gen:api); no se escriben a mano.
- Datos del servidor con TanStack Query (paginación por cursor con useInfiniteQuery). Estilos con CSS Modules y variables CSS; sin librerías de UI pesadas. Justifica en docs/decisiones.md cada dependencia nueva.
- Reproductor: Shaka Player. Interfaz en español.
- Diseño modular: componentes pequeños, hooks reutilizables, un cliente API tipado por servicio (usersClient, metadataClient, streamingClient). Cada método llama a un operationId del contrato; no se inventan endpoints. Sin código duplicado ni valores mágicos.
- Variables de entorno: VITE_USERS_API_URL, VITE_METADATA_API_URL, VITE_STREAMING_API_URL, VITE_USE_MOCKS (documentadas en frontend/.env.example).
- Los errores de los APIs tienen la forma {code, message, details, traceId}: mapea cada code a un mensaje amigable en español y muestra el traceId en pequeño.

## Autenticación y rutas
- Sin sesión, la primera pantalla es REGISTRO/LOGIN. Las demás rutas están protegidas (route guard).
- Login con username y contraseña (POST /auth/login); registro con POST /users.
- El JWT se guarda en sessionStorage (con estado en memoria) y se envía como Authorization: Bearer en todas las peticiones, incluidas las de Shaka. Documenta la decisión y sus riesgos en docs/decisiones.md. Al recargar, restaura el token y valídalo con GET /users/me.
- No existe refresh de token. Si el JWT expira o cualquier API responde 401: limpiar sesión, avisar al usuario y volver al login conservando la ruta a la que iba.
- Rutas: /login, /register, /videos (lista), /videos/mine, /videos/new, /videos/:id (reproducción), /videos/:id/edit, /profile y una página 404. "/" redirige a /videos con sesión y a /login sin sesión.

## Pantallas
1. Registro y login: formularios con validación (username, email, contraseña con reglas visibles y confirmación). Errores como USERNAME_EXISTS o EMAIL_EXISTS junto al campo correspondiente.
2. Cabecera (autenticado): logo, caja de búsqueda, botón Subir video y menú de usuario (Mi perfil, Mis videos, Cerrar sesión). La cabecera no muestra cuota.
3. Lista de videos (/videos): rejilla responsive de tarjetas con miniatura, título, autor (ownerUsername), fecha de carga, duración (mm:ss) y, en videos propios, insignia Público/Privado y estado (procesando/listo/falló). Controles de orden: campo (Título, Autor, Fecha de carga) y dirección (asc/desc). Búsqueda con debounce (parámetro q). El orden y la paginación se resuelven SIEMPRE en el API de metadata (sortBy, order, cursor, pageSize); al cambiar orden o búsqueda se reinicia el cursor. Botón "Cargar más" accesible por teclado. Estados: cargando (skeletons), vacío, error con reintentar.
4. Reproducción (/videos/:id): componente ShakaPlayer con adaptación automática de resolución y selector manual de calidad (Auto + resoluciones disponibles). Un request filter de Shaka agrega el JWT al manifiesto y a los segmentos. Debajo: título, descripción, autor, tema, etiquetas, fecha y duración. Si el usuario es dueño: Editar, Cambiar visibilidad y Eliminar (con confirmación). Manejar: video en procesamiento (mensaje y sondeo de GET /videos/{id}/processing), 403, 404 y error de reproducción. Destruir el reproductor al desmontar.
5. Subir video (/videos/new): paso 1, metadata (título, descripción, autor por defecto = username, tema, etiquetas como chips, visibilidad); paso 2, archivo con validación de tipo y tamaño. Flujo: POST /videos -> POST /videos/{videoId}/upload (devuelve uploadId) -> PUT /uploads/{uploadId}/chunks/{n} por fragmento con el header X-Chunk-Checksum -> POST /uploads/{uploadId}/complete con fileChecksum -> sondear el procesamiento hasta ready o failed. Con barra de progreso, cancelar (DELETE /uploads/{uploadId}) y reanudar (GET /uploads/{uploadId}). Checksums: SHA-256 en hexadecimal minúscula (por fragmento y del archivo completo); usa crypto.subtle y, si no está disponible (por ejemplo http fuera de localhost), una librería JavaScript de respaldo. Estados de subida: uploading, queued, processing, published, failed. Si el archivo no cabe en la cuota restante, avisar antes de subir y deshabilitar el botón; si el servidor responde QUOTA_EXCEEDED, explicarlo.
6. Mis videos (/videos/mine): GET /videos?owner=me, con estado, visibilidad y acciones (Reproducir, Editar, Eliminar).
7. Editar video (/videos/:id/edit): editar título, descripción, autor, tema, etiquetas y fecha de carga (PATCH /videos/{id}) y visibilidad (PATCH /videos/{id}/visibility). Duración y tamaño solo lectura. Solo accesible para el dueño.
8. Mi perfil (/profile): datos del usuario, barra de cuota (GET /users/me/quota) que NO se puede editar y que solo se muestra en esta pantalla, editar email (PATCH /users/{id}), eliminar cuenta con confirmación fuerte (DELETE /users/{id}). El username es inmutable.

## Miniaturas autenticadas
Las etiquetas img no pueden enviar headers. Crea un hook useAuthedImage que descargue la miniatura (GET /videos/{videoId}/thumbnail) con fetch + Authorization, la convierta en blob URL, la libere al desmontar y muestre un marcador si falla.

## UX transversal
- Responsive (móvil, tablet, escritorio), tema claro/oscuro según prefers-color-scheme, estilo sencillo tipo YouTube.
- Accesibilidad básica: HTML semántico, labels en formularios, foco visible, navegación por teclado, contraste suficiente, aria-live para errores y progreso, alt en imágenes.
- Fechas con Intl.DateTimeFormat en es-CR. Notificaciones (toasts) para éxito y error.

## Modo mock (desarrollar sin backends)
Con VITE_USE_MOCKS=true usa MSW (service worker en frontend/public/) para simular los 3 APIs según los contratos: usuarios de ejemplo, 25+ videos con distintos autores, fechas, visibilidades y estados; orden, búsqueda q, owner=me y paginación por cursor reales; cuota simulada; subida simulada con progreso; errores con el formato común. Para reproducción en mock usa un manifiesto DASH de ejemplo documentado. Los mocks se cargan con import dinámico y no deben entrar al build de producción.

## Pruebas
Vitest + React Testing Library + MSW, desde la primera sub-fase: httpClient (parseo de errores y 401), authStore, login y route guard, lista (orden, búsqueda, cargar más), subida con bloqueo por cuota, checksum, useAuthedImage, componente del reproductor (con Shaka simulado). Las pruebas no dependen de los backends reales.

## Estructura sugerida (frontend/src/)
api/ (httpClient, errors, usersClient, metadataClient, streamingClient), app/ (App, routes), auth/ (AuthContext, authStore, tokenStorage), components/ (AppLayout, Header, VideoCard, VideoGrid, SortControls, QuotaBar, ShakaPlayer, UploadWizard, TagInput, ConfirmDialog, Toast, LoadingState, ErrorState, ProtectedRoute), hooks/, mocks/, pages/, styles/, types/ (generated/), utils/ (checksum, dates, duration, validation).

## Forma de trabajo
1. Sub-fases, una a la vez: F1 base (router, layout, tema, cliente HTTP y errores, tres clientes, auth store, mocks, páginas navegables); F2 registro, login, guard y perfil; F3 lista con orden, búsqueda y paginación; F4 reproducción; F5 subida y gestión de videos; F6 pulido (errores, accesibilidad, más pruebas, README del frontend).
2. Antes de tocar archivos de cada sub-fase, muéstrame el plan y la lista exacta de archivos que se crean o reemplazan. Espera mi validación.
3. Al terminar cada sub-fase: build, pruebas, git status, commit y un resumen de lo que el equipo debe entender para defenderla.
4. Entorno: Windows con PowerShell, usa npm.cmd. Muestra el comando completo antes de ejecutarlo y no borres ni sobrescribas archivos sin decir cuáles.
5. No agregues dependencias sin justificarlas.

## Criterios de aceptación
Con VITE_USE_MOCKS=true puedo: registrarme, iniciar sesión, ver una lista paginada, ordenarla por título/autor/fecha, buscar, reproducir un video, subir uno (simulado), editar su metadata, cambiar su visibilidad, eliminarlo y ver mi cuota. Con los backends reales y VITE_USE_MOCKS=false, el mismo flujo funciona de extremo a extremo.