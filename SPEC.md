# Tecflix – SPEC.md (Versión 1.0)

## Rol
Actúa como arquitecto de software senior y desarrollador full-stack con experiencia en sistemas distribuidos, C++20, React/TypeScript, MongoDB, estructuras de datos en disco (árboles B) y observabilidad con OpenTelemetry. Los requerimientos son deliberadamente de alto nivel: investiga, propone decisiones justificadas y aplica buenas prácticas donde este documento no especifique detalles.

## Contexto y alcance
Proyecto del curso Algoritmos y Estructuras de Datos II (CE 2103), TEC. Este archivo es la fuente de verdad para el agente de IA (spec-driven development). Alcance: SOLO v1.0 (tag v1.0). Fuera de alcance ahora: redundancia de servidores, optimización, load-test, GitHub pipelines y Playwright (V2.0). Aun así, diseñar sin cerrar puertas: configuración por variables de entorno, dependencias inyectadas y servicios sin estado local (excepto el archivo de usuarios).

## 1. Componentes (todos deben funcionar integrados vía Docker Compose)
1. Aplicación web: React + TypeScript (Vite).
2. API de usuarios: REST en C++20.
3. API de metadata: REST en C++20 + MongoDB.
4. API de streaming: C++20; recibe subidas, transcodifica con FFmpeg y sirve MPEG-DASH.
5. Dashboard del sistema: OpenTelemetry en los 3 APIs -> OTel Collector -> Aspire Dashboard (traces, logs) y Prometheus + Grafana (métricas).

## 2. Reglas de ingeniería (obligatorias)
- POO: clases de una sola responsabilidad, interfaces abstractas (IUserRepository, IVideoRepository, ITranscoder, IClock...), inyección de dependencias por constructor, composición sobre herencia. Patrones solo si se justifican.
- Cero malas prácticas: RAII y smart pointers (sin new/delete crudos), sin globales ni singletons mutables, sin números/strings mágicos, sin código duplicado ni muerto, funciones cortas, const-correctness, errores manejados explícitamente, sin secretos en código. clang-format y clang-tidy (C++ Core Guidelines) sin advertencias. TypeScript en modo strict y sin any.
- Código defendible: 4 personas deben poder explicarlo. Comentar el porqué, no el qué. Cada decisión importante va en docs/decisiones.md.
- Diseñar para pruebas unitarias (V2 exigirá 90% de cobertura): escribir pruebas desde ya con GoogleTest (C++) y Vitest (frontend).
- Prueba de humo de extremo a extremo: registro -> login -> subir -> transcodificar -> listar -> reproducir.
- Logging estructurado JSON, validación de entradas, README por componente.

## 3. API de usuarios
- Operaciones: registrar, consultar, actualizar y eliminar usuarios.
- Login devuelve un JWT HS256 con duración configurable por variable de entorno. No hay endpoint de refresh: al expirar, el cliente vuelve a iniciar sesión.
- Almacenamiento: archivo binario propio users.dat (sin motor de BD) + índice de árbol B en disco users.idx implementado a la medida (páginas de 4096 bytes, orden derivado del tamaño real de página, claves username y email apuntando al offset del registro). Operaciones: insertar, buscar, actualizar, eliminar (borrado lógico), recorrido en orden.
- Documentar en docs/btree.md: estructura del registro binario, formato de página del árbol B, crecimiento del archivo (bloques configurables, lista de espacio libre), concurrencia (shared_mutex + bloqueo de archivo del SO) y recuperación ante fallos (WAL, checksums CRC32C, reconstrucción del índice).
- Pruebas del árbol B: inserciones masivas, splits, merges, persistencia tras reinicio, reconstrucción.
- Contraseñas con Argon2id; nunca se devuelve el hash.
- El username es INMUTABLE en v1 (evita inconsistencias con ownerUsername en metadata).
- Privacidad: a otros usuarios solo se les muestran campos públicos (id, username); el email solo lo ve el propio usuario.
- Cuota: se asigna al registrarse (valor por defecto configurable, p. ej. 5 GB) y es INMUTABLE: no existe endpoint ni operación para modificarla, ni siquiera para administradores. El registro guarda quotaBytes y usedBytes.
- Reservas de cuota: endpoints internos (solo con token de servicio, distinto del JWT de usuario) para reservar, ajustar (settle) y liberar, idempotentes por reservationId. Las reservas se guardan en un registro persistente (reservationId, userId, bytes, estado, createdAt, expiresAt) con expiración y limpieza de reservas abandonadas. Al eliminar un video ya publicado se libera su espacio de forma definida en el contrato.

## 4. API de metadata
- MongoDB, colección videos: id (UUID), ownerId, ownerUsername (desnormalizado para ordenar por autor), title, description, author (editable), topic, tags, durationSeconds, uploadedAt, visibility (public|private), sizeBytes, status (processing|ready|failed), createdAt, updatedAt.
- Operaciones: crear (status processing), consultar, actualizar metadata (solo dueño; campos editables: title, description, author, topic, tags, uploadedAt), cambiar visibilidad, eliminar (solo dueño; pide al streaming borrar archivos antes de confirmar y libera cuota), listar.
- Listado: GET /videos?sortBy=title|author|uploadedAt&order=asc|desc&cursor=...&pageSize=1..50&q=...&owner=me. Devuelve los videos del usuario autenticado + los públicos de otros; owner=me devuelve solo los del usuario. El orden por autor usa ownerUsername. Orden y paginación SE HACEN EN MongoDB con índices compuestos y desempate por _id; nunca se trae la lista completa. Respuesta: items + nextCursor (opaco).
- Índices: por (ownerId, campo, _id) y (visibility, campo, _id) para title, ownerUsername y uploadedAt; búsqueda con índice de texto solo sobre title, tags y topic (NO comodín $**). Collation insensible a mayúsculas en índices y consultas. Validar con explain() que el listado usa índices.
- Usuario no autenticado no puede usar el API de metadata.
- Endpoints internos (token de servicio): processing-complete, processing-failed, streaming-deleted.

## 5. API de streaming
- Pipeline de ingestión: subida -> validación y reserva de cuota -> cola -> transcodificación -> publicación de segmentos -> notificación a metadata. Cola en memoria en v1 detrás de una interfaz sustituible; videos incompletos quedan como processing y se pueden reintentar.
- Antes de iniciar una subida, verificar con metadata que el video existe, pertenece al usuario del JWT, está en status=processing y no tiene otra subida activa.
- Subida reanudable: iniciar, enviar fragmentos con checksum obligatorio por fragmento (header definido en el contrato), consultar estado, completar y cancelar. Los fragmentos se escriben a disco, nunca se carga el video completo en memoria. Se valida tamaño, secuencia, checksum de fragmento y checksum final.
- Cuota: reserva inicial = tamaño original + multiplicador configurable para transcodificados + miniatura; al terminar se ajusta (se libera la diferencia). Si falla FFmpeg: se eliminan artefactos parciales, se libera la reserva y se marca failed. Si no cabe, QUOTA_EXCEEDED (403/413).
- Transcodificación: FFmpeg a MPEG-DASH (fMP4, segmentos de 4 s, representaciones 240p-1080p sin escalar por encima del original, audio AAC), miniatura y duración. Asíncrona con pool de workers limitado. Interfaz ITranscoder con implementación FfmpegTranscoder; FFmpeg se ejecuta con argumentos separados, nunca por concatenación de texto del usuario.
- Reproducción compatible con Shaka Player / Dash.js: manifest.mpd (application/dash+xml), segmentos (video/mp4, audio/mp4), HTTP Range, CORS restringido a la URL del frontend, Cache-Control para segmentos inmutables.
- Autorización: JWT en header Authorization (filtro de requests de Shaka). Videos privados solo para su dueño; públicos para cualquier usuario autenticado.
- Miniatura servida por el API de streaming, protegida con JWT.

## 6. Aplicación web
- Primera pantalla: formulario de registro / autenticación. Resto de rutas protegidas.
- Lista de videos paginada, ordenable por título, autor y fecha de carga (asc/desc), con búsqueda y miniaturas. Orden y paginación vienen del API de metadata.
- Al elegir un video: reproducirlo (Shaka Player) o gestionar su metadata (solo dueño).
- Subir con progreso, cancelar y reanudar; estado de procesamiento; eliminar; editar metadata; cambiar visibilidad; duración solo lectura.
- Barra de cuota; subida deshabilitada al alcanzarla (también se valida en backend).
- Las miniaturas se descargan con fetch + Authorization y se muestran con blob URL (las etiquetas img no envían headers).
- Si el JWT expira o hay un 401: cerrar sesión, avisar y volver al login conservando la ruta.
- Resolución adaptable (automática + selector manual), responsive, cliente API tipado por servicio, interfaz en español, accesibilidad básica.
- Modo mock (VITE_USE_MOCKS=true) con MSW basado en los contratos, para desarrollar sin backends.

## 7. Observabilidad (OpenTelemetry)
- Traces con propagación W3C traceparent: acción del usuario -> API -> almacenamiento, y entre APIs. Logs de error con trace_id y span_id.
- Métricas mínimas: http_requests_total, http_request_errors_total, http_request_duration_seconds, users_btree_search_duration_seconds, users_disk_pages_read_total, users_created_total, users_deleted_total, metadata_mongo_query_duration_seconds, metadata_video_list_duration_seconds, streaming_uploads_started_total, streaming_uploads_quota_rejected_total, streaming_transcode_queue_size, streaming_transcode_duration_seconds, streaming_bytes_served_total, streaming_segments_served_total, service_cpu_usage, service_memory_usage, service_disk_usage, storage_used_bytes, storage_quota_bytes.
- Usuario activo: con una solicitud autenticada exitosa en los últimos 15 minutos.
- /health y /ready por API (revisan Mongo, archivo de usuarios, FFmpeg, disco).
- Aspire Dashboard (trazas y logs) + Prometheus + Grafana (métricas y dashboards provisionados: errores, salud, desempeño, estadísticas generales). Justificar en docs/observabilidad.md.

## 8. Forma de trabajo
1. Fase F0 (primero): SPEC.md y contratos OpenAPI 3.0.3 en docs/openapi/ (users.yaml, metadata.yaml, streaming.yaml). Frontend y mocks se generan a partir de ellos.
2. Fases: A esqueleto, CMake, vcpkg, Compose y telemetría base; B archivo de usuarios + árbol B + WAL; C Argon2id, JWT, registro/login y cuota; D metadata + MongoDB; E streaming, subida, cola, FFmpeg y DASH; F frontend; G observabilidad; H integración E2E, documentación y tag v1.0.
3. Una fase a la vez. Al final de cada fase: compilar, correr pruebas, commit y resumen de lo que el equipo debe entender para defenderla.
4. Si algo es ambiguo, pregunta o declara el supuesto y por qué. En alternativas razonables, explica el trade-off y recomienda.
5. Entorno: Windows con PowerShell. Muestra el comando completo antes de ejecutarlo y no borres ni sobrescribas archivos sin decir cuáles.
6. No agregues dependencias sin justificarlas.

## 9. Estructura de archivos
tecflix/
├── SPEC.md, README.md, docker-compose.yml, .env.example, CMakeLists.txt, vcpkg.json, .clang-format, .clang-tidy
├── docs/ (decisiones.md, btree.md, arquitectura.md, dash-streaming.md, observabilidad.md, openapi/)
├── services/
│   ├── common/ (JwtVerifier, TelemetrySetup, Config, tipos de error)
│   ├── users-api/ (src/: main.cpp, api/, domain/, auth/, quota/, telemetry/, storage/ -> UserFile, BTree, BTreePage, FileUserRepository; tests/)
│   ├── metadata-api/ (src/: main.cpp, api/, domain/, auth/, telemetry/, storage/mongo/; tests/)
│   └── streaming-api/ (src/: main.cpp, api/, domain/, telemetry/, transcoding/ -> ITranscoder, FfmpegTranscoder, TranscodeQueue, clients/ -> UsersClient, MetadataClient, storage/; tests/)
├── frontend/ (React + TS + Vite; src/: api/, auth/, components/, hooks/, mocks/, pages/, types/, styles/, utils/)
├── observability/ (otel-collector.yaml, prometheus.yml, grafana/)
├── data/ (users.dat, users.idx)
├── storage/ (uploads/, dash/, thumbnails/)
└── scripts/ (setup, seed, rebuild-index, smoke-test)

## 10. Decisiones validadas (contratos v1)
- Base: http://localhost:{port}/api/v1. Error común: { code, message, details, traceId }. Códigos HTTP y de error: 400 VALIDATION_ERROR/INVALID_CURSOR; 401 MISSING_TOKEN/INVALID_TOKEN/TOKEN_EXPIRED/INVALID_CREDENTIALS; 403 FORBIDDEN/QUOTA_EXCEEDED; 404 NOT_FOUND; 409 CONFLICT/USERNAME_EXISTS/EMAIL_EXISTS/INVALID_STATE; 413 PAYLOAD_TOO_LARGE/QUOTA_EXCEEDED; 415 UNSUPPORTED_MEDIA_TYPE; 422 UNPROCESSABLE_ENTITY; 500 INTERNAL_ERROR; 502 DEPENDENCY_ERROR; 503 SERVICE_UNAVAILABLE. POST /auth/login usa INVALID_CREDENTIALS para credenciales incorrectas con un mensaje genérico que no revela si el usuario existe.
- Users API: POST /users, POST /auth/login, GET /users/me, GET /users/me/quota, GET /users/{id}, GET /users?username=&email=&pageSize=&cursor=, PATCH /users/{id} (no modifica username, quotaBytes, usedBytes ni passwordHash), DELETE /users/{id}, GET /health, GET /ready. Internos: POST /internal/quota/reservations, POST /internal/quota/reservations/{reservationId}/settle, POST /internal/quota/reservations/{reservationId}/release.
- Metadata API: POST /videos, GET /videos/{id}, PATCH /videos/{id}, PATCH /videos/{id}/visibility, DELETE /videos/{id}, GET /videos (con sortBy, order, cursor, pageSize, q, owner=me). Internos: POST /internal/videos/{id}/processing-complete, /processing-failed, /streaming-deleted.
- Streaming API: POST /videos/{videoId}/upload, PUT /uploads/{uploadId}/chunks/{chunkNumber}, GET /uploads/{uploadId}, POST /uploads/{uploadId}/complete, DELETE /uploads/{uploadId}, GET /videos/{videoId}/manifest.mpd, GET /videos/{videoId}/dash/{path}, GET /videos/{videoId}/thumbnail, GET /videos/{videoId}/processing, GET /health, GET /ready.
- Identificadores UUID. Cola de transcodificación en memoria. Eliminación lógica primero y física coordinada por streaming. HTTP interno en Compose (TLS en un reverse proxy más adelante). pageSize máximo 50.