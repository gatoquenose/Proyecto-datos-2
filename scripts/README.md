# Scripts (PowerShell)

Se ejecutan desde la raiz del repositorio. En Windows, si el sistema bloquea los scripts, se usa:
`powershell -ExecutionPolicy Bypass -File .\scripts\<nombre>.ps1`

| Script | Para que sirve |
|---|---|
| `setup.ps1` | Deja todo listo desde cero: comprueba Docker y Git, crea `.env` desde `.env.example`, construye las imagenes y levanta los servicios. `-SoloPreparar` solo crea `.env`. |
| `seed.ps1` | Crea usuarios de ejemplo (`demo1`...`demoN`) por la API de Users. `-Cantidad 5`, `-Clave ...`. Salta los que ya existen. |
| `rebuild-index.ps1` | Reconstruye `users.idx` desde `users.dat` (detiene Users, corre `users-api --rebuild-index` y lo reinicia). |
| `smoke-test.ps1` | Prueba de punta a punta: registro, login, cuota, crear video, subida por fragmentos, transcodificacion y manifiesto DASH. `-VideoPath` activa la subida. |
