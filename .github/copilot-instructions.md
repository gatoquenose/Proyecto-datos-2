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

F0 a F4 están hechos: SPEC y contratos validados, frontend base, autenticación
y perfil, lista con búsqueda/orden/paginación/miniaturas, y reproducción DASH
con fixture sintético, sondeo y acciones del dueño. El backend sigue pendiente
desde el esqueleto de la fase A.

El siguiente paso es F5, subida y gestión. Su plan aprobado exige checksum
incremental con `sha256.create().update(...)` en una sola pasada leyendo
`Blob.slice` en orden, SHA-256 por fragmento, concurrencia 1 en v1,
reanudar con `sessionStorage`, reintentar solo red y 5xx, cuota con
`QUOTA_EXCEEDED` como fuente de verdad, invalidar `["quota"]`, limpiar
registros huérfanos con `deleteVideo`, variable `VITE_MAX_UPLOAD_BYTES`,
asistente de dos pasos, polling de procesamiento, `beforeunload`, edición y
mocks/pruebas correspondientes. F6 queda después de F5; el backend comienza
después en el orden A, B, C, D, E, G y H. No iniciar F6 ni backend durante F5.
