# Traspaso a Claude Code

## Estado verificado

- Rama: `frontend-f1` (publicada en origin).
- Último commit del frontend: `d6e33a7` (`Corrige la lista de prueba manual con mocks`).
- Árbol limpio.
- Baseline verificada: lint y build pasan; 218 pruebas en 24 archivos; 0 avisos
  `act(...)`. El build conserva la advertencia de Vite por el chunk de Shaka
  mayor a 500 kB.
- F0–F6 están hechos. Backend: fase A hecha; los APIs avanzan en ramas de los
  compañeros (ver «Contratos y backend»).

## Subfases F6

| Subfase | Estado | Commit y alcance |
|---|---|---|
| F6-1 | Hecha | `1265b5f`: consolidar `DashPlayer.test.ts` y `DashPlayer.test.tsx`, conservando las aserciones. |
| F6-2 | Hecha | `efc7d91`: eliminar los avisos `act(...)` en VideoCard, ProtectedRoute y ProfilePage solo desde el arnés. |
| F6-3 | Parcial | `0c31865`: campos legibles en tema oscuro; `3f55a1d`: una sola alerta de error al guardar; `cbf3c4f`: mensaje de restauración inválida probado y documentado; `c3223b4`: decisión de conservar los precargados en processing documentada. F6-3a y F6-3d están aplazadas. |
| F6-4 | Hecha | `b43ad94` y `a3901b8`: README del frontend. |
| F6-5 | Hecha | `d6e33a7`: lista de comprobación manual (`docs/prueba-manual-mocks.md`), ejecutada con mocks sin fallos. |
| F6-6 | Hecha | `4fd14f9`: miniatura mock PNG 1×1 válida como constante base64 en `handlers.ts`, con su prueba. |

F6-3 agregó una prueba: `presenta un solo mensaje accesible cuando falla el
guardado`. Esta falló primero con dos alertas y pasó con una. La prueba de
restauración inválida usa un 401 explícito y comprueba el mensaje y la limpieza
de sesión. F6-6 agregó la prueba `devuelve una miniatura PNG 1x1 válida`, que
comprueba la firma PNG, el tamaño, el CRC de cada chunk y el chunk final IEND.
La suite quedó en 218/24/0.

## Pendientes y decisiones aplazadas

- **F6-3a:** foco después de guardar o volver. Producción no lo mueve y el SPEC
  no lo exige; el usuario decidió dejarlo pendiente.
- **F6-3d:** `returnTo` debe conservar `pathname + search` y validarse para
  aceptar solo destinos same-origin; aplazado por decisión del usuario.
- Añadir una prueba directa del mensaje inicial de un video cuyo estado es
  `failed`: «El procesamiento del video falló.». La prueba existente cubre la
  detención del sondeo ante `failed`, no ese mensaje.
- Comprobar visualmente en navegador los campos del tema oscuro. Contraste
  calculado: 11.86:1 para texto y 3.08:1 para borde; no hay prueba automática
  de color.

## Decisiones que no deben reabrirse

- La cuota se muestra únicamente en `/profile`; conservar la caché `["quota"]`
  y su invalidación.
- Los seis videos mock precargados en `processing` se dejan así. La decisión
  está documentada en `docs/decisiones.md`; los mocks no avanzan esos registros
  porque el handler solo progresa videos con una subida asociada.
- Los mocks se usan con `npm.cmd --prefix frontend run dev`; `vite preview` no
  los carga.

## Contratos y backend

Los pendientes de contrato siguen en `SPEC.md` §13: eliminación de cuentas con
videos (propuesta 409 `INVALID_STATE`), base relativa de segmentos DASH en el
manifiesto, tamaño máximo de subida y multiplicador de reserva, máximo de
`chunkSize` (propuesta 64 MiB; el YAML solo define mínimo 1), rotación de clave
JWT, borrar videos `processing` sin archivos, y reintentar un 400 de
`completeUpload` con el mismo checksum sin iniciar ni cancelar otra subida ni
reenviar fragmentos, conservando la reserva.

Estado del backend: la fase A está hecha. Los compañeros trabajan en las ramas
`feature/users-api`, `feature/metadata-api` y `feature/streaming-api`, y la
rama `feature/integracion-tecflix` las une con el frontend. Pendiente:
integración E2E sin mocks (registro, login, subir, transcodificar, listar,
reproducir), observabilidad (G) y documentación (H). Orden definido: A, B, C,
D, E, G y H.

## Trampas del entorno

- Windows con PowerShell 5.1: no usar sintaxis bash; utilizar `npm.cmd`.
- En PowerShell, entrecomillar con comillas simples los textos que contienen
  `$`; para `npx`, entrar a `frontend` con `Push-Location` y salir con
  `Pop-Location`.
- No editar con `Get-Content`/`Set-Content`; preservar UTF-8 y saltos de línea.
- No modificar líneas existentes que contengan `Authorization` o `Bearer`;
  si una debe cambiar, avisar al usuario.
- Mostrar el comando completo antes de ejecutarlo. No usar comandos
  interactivos, `--force` ni `git clean`.

## Backend fase A (hecha)

- Build: `docker compose -p tecflix build` compila los tres APIs con Docker (Dockerfile multi-etapa, vcpkg fijado con builtin-baseline).
- Pruebas: `docker build --target builder -t tecflix-builder-test .` y luego `docker run --rm tecflix-builder-test ctest --test-dir build --output-on-failure` (12 pruebas GoogleTest; el build también las ejecuta).
- Rutas: `/api/v1/health` y `/api/v1/ready` responden 200; cualquier otra ruta responde 404 con el formato común y un `traceId` generado. `/ready` aún no comprueba dependencias.
- Configuración: el puerto sale de `TECFLIX_PORT` (por defecto 8080) mediante `IEnvironment`; un valor inválido termina el servicio con un mensaje en español.
- Pendiente de la fase A: logs JSON estructurados, el resto de variables de entorno y la telemetría OpenTelemetry (aplazada a la fase G).
- Docker: ejecutar siempre con `-p tecflix`; no usar `docker system prune`, `docker stop` ni `docker rm` fuera de ese proyecto.
