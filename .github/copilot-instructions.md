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

F0 a F5 están hechos. F5 incluye SHA-256 incremental del archivo y por fragmento en hexadecimal minúsculo, concurrencia 1, reanudación con `sessionStorage`, reintentos solo por red y 5xx, cuota con `QUOTA_EXCEEDED` como fuente de verdad, limpieza de registros huérfanos, asistente y edición probados, y mocks de subida, segmentos DASH y edición. Un 400 de `completeUpload` conserva estado y reserva y permite reintentar o cancelar; solo el éxito lo deja fuera de cancelación. `VITE_MAX_UPLOAD_BYTES` existe porque los YAML no definen el máximo.

Al cierre de F5: 216 pruebas en 25 archivos y 14 avisos `act(...)` —VideoCard 2, ProtectedRoute 6 y ProfilePage 6.

Siguiente: F6 mínimo — README del frontend, revisión de accesibilidad, unir las pruebas DashPlayer duplicadas, limpiar esos avisos y prueba manual completa con mocks. Evaluar los candidatos de F6 registrados en `SPEC.md`, incluida la miniatura mock inválida. Después comienza el backend en orden A, B, C, D, E, G y H. No iniciar F6 ni backend sin aprobación. La cuota solo se muestra en `/profile`; conservar la caché `["quota"]` y su invalidación.
