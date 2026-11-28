# users-api

Servicio de usuarios de Tecflix (C++20). Registra y autentica usuarios, guarda sus datos en un archivo
propio con índice de árbol B, y lleva la cuota de almacenamiento que usa Streaming. El contrato está en
`docs/openapi/users.yaml` (si algo no coincide, manda el YAML).

## Qué hace
| Endpoint | Para qué |
|---|---|
| `POST /api/v1/users` | Registro (contraseña con Argon2id) |
| `POST /api/v1/auth/login` | Devuelve un JWT HS256 (`sub`, `username`, `iat`, `exp`) |
| `GET/PATCH /api/v1/users/me`, `GET /users/me/quota` | Perfil y cuota del usuario autenticado |
| `GET/PATCH/DELETE /api/v1/users/{id}` | Operaciones sobre un usuario concreto |
| `POST /api/v1/internal/quota/reservations` (+ `settle`, `release`) | Reservas de cuota; solo Streaming, con `SERVICE_TOKEN` |
| `GET /api/v1/health`, `GET /api/v1/ready` | Vivo / listo para atender (`/ready` prueba el archivo de usuarios) |

Todos los errores salen como `{ code, message, details, traceId }`. El servidor responde CORS para `FRONTEND_ORIGIN`.

## Variables de entorno
| Variable | Por defecto | Notas |
|---|---|---|
| `JWT_SECRET` | (obligatoria) | Mínimo 32 caracteres |
| `SERVICE_TOKEN` | (obligatoria) | Mínimo 32 caracteres; lo usa Streaming |
| `USERS_API_PORT` | `8080` | |
| `USERS_DATA_DIR` | carpeta `data` | Aquí viven `users.dat`, `users.idx`, `reservations.*` y `users.wal` |
| `JWT_EXPIRATION_SECONDS` | `3600` | |
| `DEFAULT_QUOTA_BYTES` | 5 GiB | Cuota de cada usuario nuevo |
| `FRONTEND_ORIGIN` | `http://localhost:5173` | Origen permitido por CORS |
| `RESERVATION_TTL_SECONDS` | 86400 (1 día) | Vencimiento de una reserva de cuota |
| `RESERVATION_SWEEP_SECONDS` | 60 | Cada cuánto se barren las reservas vencidas |
| `OTEL_EXPORTER_OTLP_ENDPOINT` | vacío = sin telemetría | Ver `docs/observabilidad.md` |

`.env.example` en la raíz trae valores válidos para desarrollo.

## Compilar y probar
```powershell
$env:VCPKG_ROOT="C:\vcpkg"
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug --output-on-failure
```
En Linux (CI y Docker) se usa GCC 12; por eso no se usa `<format>`.

## Ejecutar
```powershell
$env:JWT_SECRET="un-secreto-de-al-menos-32-caracteres!!"
$env:SERVICE_TOKEN="otro-secreto-de-al-menos-32-caracteres!"
.\build\windows-debug\services\users-api\users-api.exe
```
Mantenimiento: `users-api --rebuild-index` rehace `users.idx` desde `users.dat` y termina.

## Cómo está hecho (resumen)
- **Almacenamiento:** `users.dat` con registros fijos de 384 bytes; `users.idx` es un árbol B en disco (páginas de 4096 bytes)
  con claves prefijadas `u:` (usuario), `e:` (correo) e `i:` (id). Borrado lógico con marca.
- **Integridad:** CRC32C en cada página y en cada registro; si algo no cuadra se lanza `CorruptionError`
  y al arrancar el índice se reconstruye solo desde `users.dat`.
- **Operaciones a medias (WAL):** `users.wal` guarda la intención antes de tocar los archivos y se repite al arrancar.
  Límite conocido: se hace `flush` pero no `fsync`.
- **Concurrencia:** `shared_mutex` (varios lectores, un escritor).
- **Detalle de formatos y decisiones:** `docs/btree.md`.

## Estructura
```
src/api/        controladores HTTP, CORS, errores comunes
src/service/    reglas de negocio (usuarios, cuota, barrido de reservas)
src/storage/    archivos, árbol B, CRC32C, WAL, reconstrucción del índice
src/auth/       Argon2id y generación de UUID
src/telemetry/  métricas y trazas propias de Users
tests/          GoogleTest (una prueba por comportamiento)
```

## CI
`.github/workflows/users-ci.yml` compila, ejecuta las pruebas, exige 90% de cobertura de líneas (`gcovr`) y guarda
el ejecutable y el reporte como artefactos descargables.
