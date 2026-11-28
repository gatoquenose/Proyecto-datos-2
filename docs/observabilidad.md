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

## Recorrido de las métricas
```
users-api (OTLP/HTTP :4318) -> otel-collector -> Prometheus (:9090) -> Grafana (:3000)
                                       \-> Aspire Dashboard (trazas)
```
- El Collector (`observability/otel-collector.yaml`) recibe OTLP en 4317 (gRPC) y 4318 (HTTP). Exporta métricas en formato
  Prometheus por el puerto 8889 y trazas al Aspire Dashboard. Necesita la imagen `otel/opentelemetry-collector-contrib`,
  porque la imagen base no trae el exportador de Prometheus.
- `resource_to_telemetry_conversion` está activo para que cada métrica lleve la etiqueta `service_name` y los dashboards
  puedan separar Users, Metadata y Streaming.
- Prometheus (`observability/prometheus.yml`) lee el puerto 8889 del Collector cada 15 s (trabajo `tecflix`).
- Nombres: Prometheus agrega `_bytes` a las métricas con unidad `By`. Para que `service_memory_usage` y `service_disk_usage`
  se llamen como en la SPEC, esas dos se declaran sin unidad y la unidad queda en la descripción.
- Los contadores arrancan en 0 al iniciar el servicio, así aparecen en Prometheus aunque aún no haya habido ningún evento.

## Dashboards de Grafana
Se cargan solos desde `observability/grafana/dashboards/` (aprovisionados; no se configura nada a mano). Todos tienen un
selector de servicio.

| Dashboard | Qué muestra |
|---|---|
| Tecflix - Errores | % de errores, errores por segundo, por código HTTP y rutas con más errores |
| Tecflix - Salud | Collector/Prometheus arriba, servicios reportando, memoria, CPU, disco, cola de transcodificación |
| Tecflix - Desempeño | Peticiones por segundo, latencia p50/p95/p99 por servicio y por ruta, búsqueda en el árbol B, páginas leídas/escritas, consultas Mongo, transcodificación |
| Tecflix - Estadísticas generales | Usuarios creados/eliminados, peticiones, errores, subidas, bytes y segmentos servidos, cuota usada contra asignada |

## Por qué esta pila
- **OpenTelemetry:** estándar abierto; una sola biblioteca para los 3 servicios y propagación de `traceparent` entre ellos.
- **Prometheus + Grafana:** estándar para métricas; las consultas por servicio y percentiles de latencia salen directo de los histogramas.
- **Aspire Dashboard:** recibe trazas y logs por OTLP sin configurar nada más, y es ligero para correr en Docker Compose.

## Cómo comprobar que llegan las métricas
1. `docker compose up` y esperar a que Users esté listo (`/api/v1/ready`).
2. Abrir http://localhost:9090/targets: el trabajo `tecflix` debe estar **UP**.
3. En http://localhost:9090 consultar `http_requests_total`; deben aparecer series con `service_name`.
4. Abrir http://localhost:3000 (usuario `admin`, contraseña `admin`) y entrar a la carpeta de dashboards.
