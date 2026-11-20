# Instrucciones para el agente

Antes de cualquier tarea, lee `SPEC.md` completo. Es la fuente de verdad del
proyecto; si contradice un contrato de `docs/openapi/`, prevalece el YAML.

## Reglas de trabajo

- El entorno es Windows con PowerShell, CLion y Node 22. Usa `npm.cmd`.
  Para `npx`, entra primero en `frontend` con `Push-Location` y sal con
  `Pop-Location`. Entrecomilla con comillas simples todo texto que contenga
  `$`.
- No edites archivos con `Get-Content`/`Set-Content` (en Windows PowerShell 5.1
  pueden corromper caracteres UTF-8); usa la herramienta de edición.
- Si una lectura muestra `******`, el archivo contiene otro valor: nunca lo
  copies de vuelta ni escribas `******` literal.
- No edites líneas que contengan `Authorization` o `Bearer`; si deben cambiar,
  avisa para que se modifiquen en el IDE. En código nuevo, construye el header
  con constantes como `BEARER_SCHEME` y `MOCK_TOKEN`, nunca con un literal
  completo `"Bearer xyz"`.
- Si un mismo problema falla dos veces seguidas, detente y descríbeme el
  problema y las opciones en lugar de seguir intentando.
- Antes de cada commit que toque mocks o la configuración de Vite, compila con
  `$env:VITE_USE_MOCKS='false'` y quita la variable después. Comprueba que
  `frontend\dist` no contiene chunks `browser-*.js` ni las cadenas `mock-jwt`,
  `CorrectHorse` o `ana.tec`, además de `.m4s` y `manifest.mpd`.
- Después del commit que restringe los mocks a desarrollo, repite la
  verificación con tres builds: `$env:VITE_USE_MOCKS='true'`,
  `$env:VITE_USE_MOCKS='false'` y la variable eliminada con
  `Remove-Item Env:VITE_USE_MOCKS`. En los tres casos `frontend\dist` no debe
  contener `browser-*.js`, `.m4s`, `manifest.mpd`, `mock-jwt`, `CorrectHorse`
  ni `ana.tec`; quita siempre la variable al terminar. Desde ese commit usa
  `npm.cmd --prefix frontend run dev` para los mocks; `vite preview` no los
  carga.
- Muestra cada comando completo antes de ejecutarlo y no uses comandos
  interactivos. No borres ni sobrescribas archivos sin indicar cuáles. Nunca
  uses `--force`, `--legacy-peer-deps`, `npm audit fix --force` ni `git clean`
  sin un patrón explícito.
- Antes de editar, muestra el plan y la lista exacta de archivos a crear o
  modificar y espera validación. Si hace falta tocar un archivo fuera de esa
  lista, avisa antes.
- Trabaja una subfase a la vez, con commits pequeños y separados. Antes de
  cada commit, ejecuta build, lint y pruebas sin flags; después haz `git push`.
  Cierra cada subfase con resumen, verificación, pendientes y puntos que el
  equipo debe poder defender.
- Cada comportamiento prometido debe tener una prueba. Lista las pruebas
  nuevas y el total. No debilites aserciones ni ocultes fallos de producción;
  distingue fallos del arnés de fallos de producción y detente para informar
  si una prueba descubre un bug.
- No inventes endpoints, archivos binarios ni material con derechos de autor.
  Justifica las dependencias nuevas en `docs/decisiones.md`. Ante ambigüedad,
  pregunta o declara el supuesto y su motivo; explica los trade-offs y
  recomienda cuando existan alternativas razonables.

## Estado y siguiente paso

F0–F5 están hechos. F5 incluye SHA-256 incremental del archivo y de cada
fragmento en hexadecimal minúsculo, concurrencia 1, reanudación con
`sessionStorage`, reintentos solo por red y 5xx, cuota con `QUOTA_EXCEEDED`
como fuente de verdad, limpieza de registros huérfanos, asistente y edición
probados, y mocks de subida, DASH y edición. Un 400 de `completeUpload` conserva
estado y reserva y permite reintentar o cancelar; solo el éxito lo deja fuera
de cancelación. `VITE_MAX_UPLOAD_BYTES` existe porque los YAML no definen el
máximo.

F6 está parcial. F6-1 unió las pruebas de DashPlayer (`1265b5f`); F6-2 limpió
los avisos del arnés (`efc7d91`); F6-3 completó estilos oscuros (`0c31865`),
alerta única al guardar (`3f55a1d`), mensaje probado de restauración de sesión
inválida (`cbf3c4f`) y documentación del estado de mocks (`c3223b4`). Baseline
al cierre: 217 pruebas en 24 archivos, 0 avisos `act(...)`, lint y build
verdes.

Pendiente: F6-3a (foco tras guardar o volver, aplazado porque producción no lo
hace y SPEC no lo exige); F6-3d (`returnTo` con query y validación same-origin,
aplazado); prueba directa del mensaje inicial del estado `failed`; comprobación
visual del tema oscuro. Luego quedan F6-4 (README del frontend), F6-5 (lista
de comprobación de la prueba manual con mocks) y F6-6 (miniatura mock PNG
válida de 1×1 como constante, sin binario y con tres builds). No iniciar estas
tareas ni el backend sin autorización.

La cuota se muestra únicamente en `/profile`; conservar la caché `["quota"]` y
su invalidación. Los seis videos mock precargados en `processing` se dejan así,
según `docs/decisiones.md`. Los mocks funcionan con
`npm.cmd --prefix frontend run dev`, no con `vite preview`.

Los pendientes de contrato permanecen en `SPEC.md` §13. Backend, aún no
iniciado: orden A, B, C, D, E, G y H. Ver el estado detallado y los criterios
de traspaso en `docs/HANDOFF.md`.
