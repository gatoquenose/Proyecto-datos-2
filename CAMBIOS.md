# Tecflix – Registro de cambios

Documento vivo. Cada cambio nuevo se agrega como una entrada al final de la sección "Historial", con su fecha.

## Decisiones de diseño (para pasar a `docs/decisiones.md`)

| # | Decisión | Estado |
|---|----------|--------|
| 1 | `reservationId` de cuota = `videoId`. Metadata y streaming no necesitan guardar otro identificador. | Pendiente de confirmar en equipo |
| 2 | Eliminación lógica con un campo interno `deletedAt` en metadata (no aparece en el esquema `Video`). | Pendiente de confirmar en equipo |
| 3 | `GET /users?email=` solo coincide con el email del propio usuario; el de otra persona devuelve lista vacía (evita exponer correos). | Pendiente de confirmar en equipo |
| 4 | Código de cuota insuficiente: usuarios → streaming devuelve 403 `QUOTA_EXCEEDED`; streaming → cliente devuelve 413 `QUOTA_EXCEEDED`. | Pendiente de confirmar en equipo |
| 5 | `release` de cuota funciona sobre reservas `active` y `settled`, y es idempotente. Así se libera el espacio al borrar un video publicado. | Pendiente de confirmar en equipo |
| 6 | Claims del JWT (iguales en las 3 APIs): `sub` (id UUID), `username`, `iat`, `exp`. | Pendiente de confirmar en equipo |

## Flujo de eliminación de un video

1. El dueño llama a `DELETE /videos/{id}` en metadata.
2. Metadata marca el video con `deletedAt`. El video deja de aparecer en consultas y listados.
3. Metadata llama a `DELETE /internal/videos/{videoId}` en streaming (con `serviceToken`).
4. Si streaming no responde, metadata revierte la marca y devuelve 502.
5. Si streaming acepta (202), metadata responde 204 al usuario.
6. Streaming cancela subidas o transcodificaciones activas y borra los archivos (original, segmentos DASH, miniatura).
7. Streaming llama a `POST /internal/videos/{id}/streaming-deleted` en metadata.
8. Metadata elimina el documento y libera la cuota (`release` del `reservationId = id` en la API de usuarios).

## Historial

### 2026-11-09 – Fase F0: revisión de los contratos OpenAPI

**`users.yaml`**
- `reserveQuota`: se agregan 200 (reserva ya existente, idempotencia), 400, 401, 403 `QUOTA_EXCEEDED` y 404.
- Endpoints internos de cuota: se agrega 401 (token de servicio). `settle` agrega 400, 403 y 409.
- `GET /users/{id}`: devuelve `Me` si es el propio usuario y `User` (id y username) si es otro.
- `GET /users?email=`: se documenta la restricción de privacidad.
- `release`: funciona también sobre reservas `settled` y es idempotente.
- Se documentan los claims del JWT.
- `login` agrega 400; `PATCH /users/{id}` agrega 404.
- `/health` y `/ready`: `summary`, `operationId` y etiqueta.

**`metadata.yaml`**
- `getVideo`: se agrega 403 para videos privados de otros usuarios.
- `deleteVideo`: se agregan 409 y 502, y se describe el flujo completo de borrado.
- Endpoints internos: se agregan `operationId`, 401, 400 y 409.
- `streaming-deleted`: elimina el documento y libera la cuota.
- `HealthResponse` con valores `ok` y `ready`, igual que en usuarios.
- Se documentan los claims del JWT y la convención `reservationId = videoId`.

**`streaming.yaml`**
- Nuevo endpoint `DELETE /internal/videos/{videoId}` (`deleteVideoFiles`), con `serviceToken`.
- Se agrega el esquema de seguridad `serviceToken`, que faltaba.
- Corregido error de sintaxis en el ejemplo de `Cache-Control` (ahora va entre comillas).
- `startUpload`: se agregan 401, 404, 415 y 502; se describe la verificación con metadata y la reserva de cuota.
- `uploadChunk`, `getUpload`, `cancelUpload`, `completeUpload`: se agregan 401, 403 y 404.
- Reproducción (manifest, segmentos, miniatura, processing): se agrega 401.
- `206` incluye el header `Content-Range`; `200` incluye `Accept-Ranges`.
- Se quita `chunkSizeBytes` del request; el servidor lo define y lo devuelve en `UploadStatus`.
- Se agregan `operationId` y etiquetas a todos los endpoints.

### 2026-11-10 – Configuración con `.env`

- Se usa el `.env.example` que ya existía en el repo (variables `TECFX_ENV`, `TECFX_LOG_LEVEL`, puertos de las 3 APIs, `MONGO_URI`, `OTEL_EXPORTER_OTLP_ENDPOINT`) como base.
- Se agregan al `.env.example` (con valores falsos): `JWT_SECRET`, `SERVICE_TOKEN`, `JWT_EXPIRATION_SECONDS`, `DEFAULT_QUOTA_BYTES`, `FRONTEND_URL`.
- El `.env` real vive solo en cada computadora y está en `.gitignore`. `JWT_SECRET` y `SERVICE_TOKEN` deben ser idénticos en las 4 máquinas.
- Los tres puertos internos son 8080 (dentro de Docker); Compose los publica como 8081 (usuarios), 8082 (metadata) y 8083 (streaming).
- Por preguntar al autor del `.env.example`: el prefijo `TECFX_` (¿error de tipeo de `TECFLIX_`?). Lo decide quien escribe el código que lee esas variables.

### 2026-11-12 – Fase A: esqueleto de `users-api` compilando

- `users-api` tiene su propio `CMakeLists.txt` (biblioteca `users_core`, ejecutable `users-api` y pruebas `users_tests`) y se agrega con `add_subdirectory` desde el `CMakeLists.txt` raíz. `metadata-api` y `streaming-api` siguen con el helper de Boost (`tecflix_common`) hasta que se construyan.
- Código inicial: `IEnvironment`, `ProcessEnvironment`, `Config` (lee `USERS_API_PORT` y `TECFX_LOG_LEVEL`), `HealthController` (`GET /api/v1/health`) y log JSON en `main.cpp`. 5 pruebas con GoogleTest, todas pasan.
- Decisión: **cpp-httplib** como librería HTTP de `users-api` (simple, con enrutador y pool de hilos). El helper `tecflix_http.hpp` de Fase A era un placeholder de un solo hilo, sin enrutador. Pendiente: acordar con el equipo la librería de metadata y streaming.
- `vcpkg.json`: se quitó la feature `ssl` de `boost-beast` (ya no existe en Boost 1.92 y hacía fallar vcpkg; no hace falta TLS en HTTP interno). Se agregan `cpp-httplib`, `nlohmann-json` y `gtest`.
- `CMakeLists.txt` raíz: `find_package(Boost CONFIG REQUIRED)` y `Boost::headers` (el componente `system` ya no es librería compilada); `enable_testing()`.
- `services/users-api/CMakeLists.txt`: `find_package(GTest REQUIRED)` sin `CONFIG` (el modo CONFIG falla con CMake 4.4).
- `CMakePresets.json`: se conserva el preset `windows-debug` (Ninja) y se agrega `testPresets`.
- `.clang-tidy`: se agregan `cppcoreguidelines-*` y `modernize-*` (lo pide el SPEC). `.clang-format` se mantiene como estaba en el repo.

**Cómo compilar en Windows**
1. Abrir **x64 Native Tools Command Prompt for VS 2022** y escribir `powershell`.
2. `cd` al repo y ejecutar `$env:VCPKG_ROOT = "C:\vcpkg"` (la terminal de Visual Studio lo cambia a otra copia de vcpkg).
3. `cmake --preset windows-debug`, `cmake --build --preset windows-debug`, `ctest --preset windows-debug`.

### 2026-11-12 – Fase B (parte 1): árbol B en disco

- Nuevo en `services/users-api/src/storage/`: `IPageStore` (con `MemoryPageStore` y `FilePageStore`), `BTreeLayout.hpp`, `BTreePage`, `ByteCodec.hpp` y `BTree` (insertar, buscar, recorrer en orden).
- Páginas de 4096 bytes; máximo 29 claves por nodo (orden derivado del tamaño de página, t = 15). Claves de 1 a 127 bytes; valores `uint64` (offset en `users.dat`).
- La página 0 guarda los metadatos (firma `TFXIDX01`, versión, tamaño de página y página de la raíz).
- 15 pruebas nuevas (5000 inserciones aleatorias, recorrido ordenado, persistencia al reabrir el archivo, rechazo de archivos corruptos). Total: 20 pruebas, todas pasan en Windows.
- Documentación inicial en `docs/btree.md`.
- Nota: se usaba una fecha de archivo posterior al reloj local y Ninja reconfiguraba sin parar; se corrigió ajustando la fecha de los archivos.

### 2026-11-12 – Fase B (parte 2): users.dat y repositorio de usuarios

- Nuevo `domain/`: `User` (con los largos máximos de cada campo) e `IUserRepository` (`add`, `update`, `remove`, `findById`, `findByUsername`, `findByEmail`).
- Nuevo en `storage/`: `UserRecordCodec` (registro binario de 384 bytes), `UserFile` (`users.dat`: cabecera de 64 bytes y registros uno tras otro), `FileUserRepository` (une `users.dat` con el árbol B) y `BinaryFile` (lectura/escritura por offset, compartida con `FilePageStore`).
- Árbol B: se agregan `update`, `erase` (borrado lógico con el valor reservado `kTombstone`) y la reactivación de claves eliminadas. El recorrido en orden omite las claves eliminadas.
- Un solo árbol con claves con prefijo: `u:<username>`, `e:<email>` y `i:<id>`. La clave `i:` es un agregado al SPEC: hace falta para `GET /users/{id}`, el JWT y las reservas de cuota.
- Username y email se comparan sin distinguir mayúsculas (se guardan en minúsculas en el índice, el registro conserva el original).
- `update` rechaza cambios de `username` y `quotaBytes` (cuota inmutable también a nivel de almacenamiento).
- Email limitado a 120 caracteres. Pendiente reflejarlo en `users.yaml` (`maxLength: 120`).
- 56 pruebas, todas pasan (validado en Linux; falta confirmar en Windows).

### 2026-11-12 – Fase C (parte 1): Argon2id, JWT y reglas de registro/login

- Nueva dependencia: **libsodium** (en `vcpkg.json`). Una sola librería da Argon2id (contraseñas), HMAC-SHA256 (firma del JWT), comparación en tiempo constante, base64url y números aleatorios seguros. Alternativa descartada: `jwt-cpp` + OpenSSL (mucho más pesada de compilar en Windows).
- Nueva biblioteca compartida `services/common/auth` (`tecflix_auth`): `JwtSigner` (solo lo usa users-api), `JwtVerifier` (lo usarán las 3 APIs), `JwtClaims`, `IClock`/`SystemClock`, `Base64Url` y `CryptoRuntime`. Tiene sus propias pruebas.
- El JWT solo acepta `HS256` (rechaza `alg: none`), exige firma válida y distingue token inválido de expirado. Claims: `sub`, `username`, `iat`, `exp`. El secreto debe tener al menos 32 bytes.
- `users-api`: `Argon2PasswordHasher` (hash en formato PHC, con sal aleatoria; el hash cabe en los 128 bytes del registro), `UuidV4Generator`, `UserValidation` (mismas reglas que `users.yaml`) y `UserService` (`registerUser` y `login`).
- `UserService`: la cuota inicial se asigna al registrarse y no se modifica; el login devuelve el mismo resultado si el usuario no existe o la contraseña falla, y gasta el mismo tiempo en ambos casos (hash de relleno).
- Contraseña: de 8 a 128 caracteres (el tope limita el costo de calcular el hash).
- 90 pruebas en total, todas pasan (validado en Linux; falta confirmar en Windows).

### 2026-11-12 – Fase C (parte 2): endpoints REST de users-api

- Endpoints implementados: `POST /users`, `POST /auth/login`, `GET /users/me`, `PATCH /users/me`, `GET /users/me/quota`, `GET /users` (búsqueda exacta por `username` o `email`), `GET|PATCH|DELETE /users/{id}`, y `/health`. Faltan `/ready` y los endpoints internos de reservas de cuota.
- Nuevo: `UsersController` (HTTP <-> `UserService`), `ApiResponses` (JSON y formato de error común `{ code, message, details, traceId }`), `ServerSetup` (404/500 en JSON, límite de cuerpo de 64 KiB, log JSON por petición) y constantes HTTP propias (`HttpStatus.hpp`).
- Seguridad: el token se lee de `Authorization: Bearer`; códigos `MISSING_TOKEN`, `INVALID_TOKEN` y `TOKEN_EXPIRED`. Solo el propio usuario puede modificar o borrar su cuenta (403 `FORBIDDEN`). El email y la cuota solo los ve el propio usuario. `PATCH` solo acepta `email`; `username`, cuota y uso se rechazan con 400.
- Concurrencia: el servidor atiende varias peticiones a la vez y el archivo/árbol B no admiten accesos simultáneos, así que se agrega `LockedUserRepository` (decorador con un `mutex`). Nuevo método `IUserRepository::modify` (leer-modificar-guardar atómico) para que dos peticiones no se pisen los cambios. Oportunidad de optimización para la V2: `shared_mutex` con un archivo por lector.
- `Config` ahora lee `JWT_SECRET` (obligatorio, sin valor por defecto), `JWT_EXPIRATION_SECONDS` (3600), `DEFAULT_QUOTA_BYTES` (5 GiB) y `USERS_DATA_DIR` (`data`). Los datos van en `data/users.dat` y `data/users.idx`.
- Cambios al contrato `users.yaml`: email `maxLength: 120`, contraseña `maxLength: 128`, búsqueda de `GET /users` exacta y código nuevo **`INVALID_CREDENTIALS`** (401 del login). Este último no estaba en la lista de códigos del SPEC; hay que confirmarlo con el equipo y agregarlo a la sección 10.
- 121 pruebas, todas pasan en Linux, incluidas pruebas con un servidor HTTP real. Probado también el ejecutable: registro, login, `/users/me`, persistencia al reiniciar y arranque sin `JWT_SECRET` (falla con un mensaje claro).

### 2026-11-12 – Fase C (parte 3): reservas de cuota, /ready y barrido de reservas

- Endpoints internos (protegidos con `SERVICE_TOKEN`, distinto del JWT de usuario): `POST /internal/quota/reservations`, `.../{reservationId}/settle` y `.../{reservationId}/release`. Un JWT de usuario no sirve en ellos y el `SERVICE_TOKEN` no abre los endpoints de usuario. Se agrega `GET /ready`.
- `QuotaService`: `usedBytes` = bytes publicados + bytes apartados por reservas activas. `reserve` rechaza con 403 `QUOTA_EXCEEDED` si no cabe; es idempotente por `reservationId` (misma petición = 200, otros datos = 409). `settle` ajusta al tamaño final (devuelve la diferencia o cobra el extra si cabe). `release` funciona sobre reservas activas y publicadas (así se libera el espacio al borrar un video) y es idempotente.
- Reservas abandonadas: cada reserva activa vence a las 24 h (`RESERVATION_TTL_SECONDS`, o el `expiresAt` que mande el servicio) y su espacio vuelve a la cuota. Un hilo (`ReservationSweeper`) las vence cada 60 s (`RESERVATION_SWEEP_SECONDS`) y también se revisan al arrancar. Una reserva vencida no se puede ajustar aunque el barrido aún no haya corrido.
- Persistencia: `reservations.dat` (registros de 128 bytes) + `reservations.idx` (árbol B propio). Las reservas no se borran, cambian de estado; compactar el archivo queda pendiente. Nuevo `FixedRecordFile`, que ahora comparten `users.dat` y `reservations.dat` (se quitó código repetido de `UserFile`).
- Nuevas variables: `SERVICE_TOKEN` (obligatoria, mínimo 32 caracteres), `RESERVATION_TTL_SECONDS` y `RESERVATION_SWEEP_SECONDS` (opcionales). `constantTimeEquals` pasó a ser pública en `tecflix_auth`: metadata y streaming la pueden usar para validar el token de servicio.
- Convención del contrato: `reservationId` = id del video. Fechas en formato `2026-11-12T10:30:00Z` (solo UTC con `Z`).
- Limitaciones conocidas: si el proceso se cae entre guardar la reserva y actualizar el uso del usuario, la cuota puede quedar desajustada (se resuelve con WAL y recuperación, pendientes). Todas las operaciones de cuota se hacen de una en una (un solo candado): oportunidad de optimización para la V2.
- 166 pruebas, todas pasan en Linux. Probado también el ejecutable: reservar, ajustar, liberar, persistencia al reiniciar y arranque sin `SERVICE_TOKEN`.

## Pendientes

- [ ] El equipo confirma las 6 decisiones de la tabla.
- [x] Reemplazar los 3 YAML en `docs/openapi/` y hacer commit (cierra la Fase F0).
- [ ] Copiar las decisiones confirmadas a `docs/decisiones.md`.
- [x] Instalar herramientas: Docker Desktop, Git, CMake, vcpkg, Node.js, Visual Studio Build Tools (C++20).
- [x] Crear `.env` compartido con `JWT_SECRET` y `SERVICE_TOKEN`.
- [ ] Repartir los componentes entre los 4 integrantes.
- [ ] Iniciar la Fase A (esqueleto, CMake, vcpkg, Docker Compose y telemetría base).
- [ ] Subir la rama `feature/users-api` y abrir Pull Request hacia `main`.
- [ ] Docker Compose (Fase A): `docker-compose.yml`, Dockerfile de `users-api`, MongoDB y OTel Collector.
- [x] Árbol B en disco: insertar, buscar y recorrer en orden (Fase B, parte 1).
- [x] Registro binario `users.dat` y repositorio de usuarios con índices de username, email e id.
- [ ] Recorrido por prefijo del índice (para `GET /users?username=`).
- [ ] Árbol B: borrado, bloques y espacio libre, concurrencia (`shared_mutex` + bloqueo de archivo).
- [x] Argon2id, JWT y `UserService` (registro y login).
- [x] Endpoints REST públicos de `users-api` y arranque con la configuración del entorno.
- [x] `users-api`: `/ready` y endpoints internos de reservas de cuota con `SERVICE_TOKEN`.
- [ ] Telemetría de `users-api` con OpenTelemetry (métricas y trazas del SPEC) y `trace_id` real en los errores.
- [ ] Docker Compose y Dockerfile de `users-api`.
- [ ] Agregar `INVALID_CREDENTIALS` a la lista de códigos del SPEC y `data/` al `.gitignore`.
- [ ] WAL, checksums CRC32C y reconstrucción del índice.
- [ ] Agregar `maxLength: 120` al email en `users.yaml` (y confirmarlo con el equipo).
- [ ] Anotar en `docs/decisiones.md`: cpp-httplib y el cambio de `vcpkg.json`.
