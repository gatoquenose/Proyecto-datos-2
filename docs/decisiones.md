# Decisiones aceptadas

- Upload verifies metadata ownership, published/status eligibility, and that no
  other active upload exists before allocation.
- Quota reservations are persistent records with expiry timestamps and a cleanup
  worker; reservations are released on failure/expiry.
- Deleting a published video releases its reserved quota.
- Usernames are immutable after account creation.
- The frontend fetches protected thumbnails with `Authorization` and uses Blob
  URLs; expired tokens trigger renewal before retrying.
- MongoDB indexes cover `title`, `tags`, and `topic`, use case-insensitive
  collation, and every index query is validated with `explain`.
- User endpoints return only privacy-scoped data and never expose credentials,
  private metadata, or internal identifiers unnecessarily.

## Desviaciones del contrato en V1.0 (decididas por el equipo)

- **startUpload (Streaming):** el frontend envía `sizeBytes`, `totalChunks`, `fileChecksum` y `contentType`, en lugar de `fileName`, `totalBytes` y `chunkSize` de `streaming.yaml`. Es lo que Streaming valida hoy. Se alinea el contrato en V2.
- **createVideo (Metadata):** exige `sizeBytes`, que `metadata.yaml` no pide. El frontend lo envía.
- **Tamaño de fragmento:** fijo en 4 MiB en el servidor (`kChunkBytes`); el frontend usa el mismo valor.
- **CORS en desarrollo:** Users y Metadata no responden OPTIONS. Mientras no lo implementen, el servidor de desarrollo de Vite reenvía `/svc/users`, `/svc/metadata` y `/svc/streaming` a los tres APIs, de modo que el navegador no hace peticiones cruzadas.
- **Build:** el backend se compila en Linux con enlace estático de MongoDB y `std::format` sustituido por `snprintf` en `IsoTime.cpp`, porque la imagen base trae GCC 12.
- **Tipos generados del frontend:** `frontend/src/types/generated/` refleja el contrato real de Streaming y no se regenera con `gen:api` hasta alinear los YAML.
- **Telemetría:** los servicios envían métricas OTLP al colector, que las expone a Prometheus en el puerto 8889.

## Limitaciones conocidas de V1.0

- Metadata no responde `/api/v1/health` ni `/api/v1/ready`.
- Faltan las métricas `metadata_*`, `streaming_bytes_served_total`, `streaming_segments_served_total` y `streaming_uploads_quota_rejected_total`.
- Users y Metadata no implementan CORS; en desarrollo lo cubre el proxy de Vite.
## Decisiones de Users API (V1.0)

- **Archivo propio con árbol B:** `users.dat` guarda registros de tamaño fijo (384 bytes) y `users.idx` es un árbol B en disco
  (páginas de 4096 bytes). Un solo índice con claves prefijadas (`u:` usuario, `e:` correo, `i:` id) en vez de tres índices:
  menos archivos y una sola estructura que mantener consistente. El borrado es lógico (marca en el registro).
- **Integridad con CRC32C:** cada página del árbol y cada registro llevan una suma de verificación. Si no coincide se lanza
  `CorruptionError` en lugar de devolver datos dañados. Es una suma de detección, no de seguridad.
- **El índice se puede reconstruir:** `users.dat` es la fuente de verdad. Al arrancar, si el índice falta o está dañado se
  reconstruye solo; también se puede forzar con `users-api --rebuild-index` (`scripts/rebuild-index.ps1`).
- **WAL de una sola posición (`users.wal`):** antes de modificar los archivos se anota la operación completa; al terminar se
  borra la anotación; al arrancar, una anotación pendiente se rehace (idempotente). Si aplicar falla, el repositorio se niega a
  aceptar nuevas escrituras hasta reiniciar, porque anotar otra sobrescribiría la pendiente. Límite conocido: se hace `flush`
  pero no `fsync`, así que un corte de energía en mal momento puede perder la última operación.
- **Contraseñas con Argon2id** (libsodium). El costo es deliberado: es lo que hace lenta cada operación de inicio de sesión.
- **JWT HS256 con secreto compartido** (`JWT_SECRET`): las otras APIs verifican sin llamar a Users. Alternativa descartada:
  RS256, que pide manejar un par de llaves y no aporta en un entorno donde todos los servicios son del mismo equipo.
- **Reservas de cuota idempotentes:** reservar con el mismo identificador devuelve el mismo resultado (201), como dice
  `users.yaml`, para que los reintentos de Streaming no cobren dos veces.
- **Concurrencia con `shared_mutex`:** varios lectores o un solo escritor sobre los archivos.
- **CORS dentro de la propia API** (`FRONTEND_ORIGIN`), no solo en el proxy de desarrollo, para que el navegador pueda hablar
  directo con las tres APIs.
- **Compilación compatible con GCC 12:** sin `<format>` (la imagen de Docker y el CI usan GCC 12).
- **Métricas con los nombres de la SPEC:** `service_memory_usage` y `service_disk_usage` se declaran sin unidad para que
  Prometheus no les agregue `_bytes`; los contadores arrancan en 0 para aparecer desde el primer momento.
- **Un solo `/ready` con verificación real:** responde 503 con el error común (y `traceId`) si el archivo de usuarios no responde.
