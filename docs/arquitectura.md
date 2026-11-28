# Arquitectura

Tecflix es un sistema de video bajo demanda formado por tres APIs en C++20, un frontend React y una capa de observabilidad.
Todo se levanta con `docker compose up`.

```
                       Navegador (React + Vite, :5173)
                                    |
        +---------------------------+---------------------------+
        |                           |                           |
  Users API :8081            Metadata API :8082          Streaming API :8083
  archivo propio             MongoDB (colección           archivos en disco +
  + árbol B + WAL            videos)                      FFmpeg (DASH)
        ^                       ^   |                          |
        |   cuota (interno)     |   +--- borrado ------------>+|
        +-----------------------+------------------------------+
        |                                                      |
        +---- reservar / confirmar / liberar cuota <-----------+
                                                  avisa a Metadata: procesamiento completo/fallido

  Las tres APIs --OTLP/HTTP--> OpenTelemetry Collector --> Prometheus --> Grafana
                                          \--> Aspire Dashboard (trazas y logs)
```

## Componentes
| Componente | Puerto | Responsabilidad | Almacenamiento |
|---|---|---|---|
| Users API | 8081 | Registro, inicio de sesión (JWT), perfil y cuota de almacenamiento | `users.dat` (registros fijos), `users.idx` (árbol B), `reservations.*`, `users.wal` |
| Metadata API | 8082 | Metadatos de los videos: crear, listar, buscar, actualizar, visibilidad, eliminar | MongoDB (`tecflix.videos`) con índices |
| Streaming API | 8083 | Subida por fragmentos, cola de transcodificación, DASH, miniaturas | Archivos en `STREAMING_DATA_DIR`; FFmpeg |
| Frontend | 5173 | Aplicación web (registro, lista, subida, reproducción) | Token en el navegador |
| Collector, Prometheus, Grafana, Aspire | 4318, 9090, 3000, 18888 | Trazas, métricas y dashboards | Volúmenes de cada contenedor |

## Autenticación
Users firma un JWT HS256 (`sub`, `username`, `iat`, `exp`) con `JWT_SECRET`. Metadata y Streaming solo verifican la firma
con el mismo secreto; ninguna llama a Users para validar cada petición. Las rutas `/internal/*` entre servicios usan
`Authorization: Bearer <SERVICE_TOKEN>`.

## Flujo de una subida
1. El frontend crea el video en **Metadata** (queda en estado `processing`).
2. Pide a **Streaming** iniciar la subida. Streaming **reserva cuota** en Users (`POST /internal/quota/reservations`).
3. El frontend envía fragmentos de 4 MiB (`PUT .../chunks/{n}`) y luego completa la subida.
4. La cola de Streaming transcodifica con FFmpeg a DASH y genera la miniatura.
5. Streaming **confirma la cuota** (`settle`) y avisa a Metadata (`processing-complete` o `processing-failed`); si falla,
   **libera** la reserva (`release`).
6. El video pasa a `ready` y se reproduce con el manifiesto `manifest.mpd`.

Las reservas tienen vencimiento y un barrido periódico las libera, de modo que un fallo a mitad de camino no deja cuota bloqueada.

## Eliminación
Metadata hace la eliminación lógica primero, pide a Streaming borrar los archivos y libera la cuota en Users; si algo falla,
revierte la marca. La eliminación física la coordina Streaming.

## Observabilidad
Cada API envía trazas y métricas por OTLP/HTTP al Collector (`OTEL_EXPORTER_OTLP_ENDPOINT`). El `traceparent` (W3C) se
propaga entre servicios. El detalle de métricas, dashboards y justificación está en `observabilidad.md`.

## Configuración
Todo valor que cambia entre entornos sale de variables de entorno (`.env.example` las lista): puertos, secretos, MongoDB,
origen permitido por CORS, carpetas de datos y dirección del Collector. Los servicios escuchan en `0.0.0.0` dentro de Docker.

## Documentos relacionados
`btree.md` (formato de archivos, integridad y WAL de Users), `dash-streaming.md`, `observabilidad.md`, `decisiones.md` y los
contratos en `openapi/`.
