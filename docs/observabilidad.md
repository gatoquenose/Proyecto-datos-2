# Observabilidad (OpenTelemetry)

Borrador de la Fase G para `users-api`. Metadata y streaming usan la misma biblioteca (`services/common/telemetry`).

## Decisiones
| Decisión | Elección | Motivo |
|---|---|---|
| Protocolo de exportación | OTLP sobre HTTP (puerto 4318) | OTLP/gRPC obliga a compilar gRPC, que es muy pesado en Windows |
| Biblioteca | SDK oficial `opentelemetry-cpp` (vcpkg, feature `otlp-http`) | Es el estándar; incluye propagación W3C y exportadores |
| Dónde se conoce el SDK | Solo `TelemetrySetup`; el resto usa la API | Si el SDK cambia, se toca un archivo |
| Telemetría apagada | Sin `OTEL_EXPORTER_OTLP_ENDPOINT` (o `OTEL_SDK_DISABLED=true`) | El servicio y las pruebas funcionan sin Collector |
| Trazas | Un span por petición HTTP; continúa el `traceparent` del cliente | Una acción del usuario se sigue entre APIs |
| Nombre del span | `MÉTODO /ruta/:parámetro` | Pocos nombres distintos aunque haya miles de usuarios |
| Id de traza en los errores | El campo `traceId` de cada error es el `trace_id` real | El cliente puede buscar esa petición en el dashboard |
| Errores 5xx | El span queda en estado Error y se registra un log JSON con `trace_id` y `span_id` | Lo pide el SPEC |
| Histogramas de segundos | Cubetas de 1 ms a 10 s | Los valores predeterminados están pensados para milisegundos |

## Variables de entorno
| Variable | Valor por defecto | Para qué sirve |
|---|---|---|
| `OTEL_EXPORTER_OTLP_ENDPOINT` | (vacío = apagada) | Dirección base del Collector, por ejemplo `http://otel-collector:4318` |
| `OTEL_SERVICE_NAME` | `users-api` | Nombre del servicio en los dashboards |
| `OTEL_METRIC_EXPORT_INTERVAL` | `15000` | Cada cuántos milisegundos se envían las métricas |
| `OTEL_SDK_DISABLED` | `false` | `true` apaga toda la telemetría |

## Métricas de users-api
| Métrica | Tipo | Qué mide |
|---|---|---|
| `http_requests_total` | contador | Peticiones atendidas (etiquetas: método, ruta, código) |
| `http_request_errors_total` | contador | Peticiones con 4xx o 5xx |
| `http_request_duration_seconds` | histograma | Duración de cada petición |
| `users_btree_search_duration_seconds` | histograma | Duración de una búsqueda de usuario (índice + lectura del registro), etiqueta `lookup` |
| `users_disk_pages_read_total` | contador | Páginas del índice leídas del disco |
| `users_disk_pages_written_total` | contador | Páginas del índice escritas |
| `users_created_total` | contador | Usuarios registrados |
| `users_deleted_total` | contador | Usuarios eliminados |
| `service_cpu_usage` | medidor | CPU del proceso, en núcleos (1.0 = un núcleo completo) |
| `service_memory_usage` | medidor | Memoria física del proceso, en bytes |
| `service_disk_usage` | medidor | Bytes que ocupan los archivos de la carpeta de datos |
| `storage_used_bytes` | medidor | Cuota usada por todos los usuarios |
| `storage_quota_bytes` | medidor | Cuota asignada a todos los usuarios |
| `active_users` | medidor | Usuarios con una petición autenticada exitosa en los últimos 15 minutos |

Los medidores (`service_*`, `storage_*`, `active_users`) se consultan cada vez que se exportan métricas.
`storage_*` recorre `users.dat` completo en cada consulta: con muchos usuarios conviene subir `OTEL_METRIC_EXPORT_INTERVAL`
o llevar los totales en memoria (mejora posible para la V2).

## Cómo comprobar que funciona
1. Levantar un panel que reciba OTLP, por ejemplo el Aspire Dashboard:
   `docker run --rm -it -p 18888:18888 -p 4318:18890 -e ASPIRE_DASHBOARD_UNSECURED_ALLOW_ANONYMOUS=true mcr.microsoft.com/dotnet/aspire-dashboard:latest`
2. Arrancar `users-api` con `OTEL_EXPORTER_OTLP_ENDPOINT=http://localhost:4318`.
3. Hacer algunas peticiones y abrir http://localhost:18888: en *Traces* aparece un span por petición y en *Metrics* las métricas de arriba.
4. Para ver la propagación: enviar el header `traceparent: 00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01`;
   la traza del dashboard y el `traceId` del error (si lo hay) deben empezar con `0af765...`.

## Pendiente
Configurar el OpenTelemetry Collector, Prometheus y Grafana en Docker Compose; el Collector debe recibir OTLP/HTTP en el puerto 4318.
