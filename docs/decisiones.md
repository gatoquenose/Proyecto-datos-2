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