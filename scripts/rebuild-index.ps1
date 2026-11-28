<#
 Reconstruye el indice (users.idx) a partir del archivo de datos (users.dat), por si el indice se corrompio.
 Detiene Users, corre la reconstruccion dentro de su contenedor y lo vuelve a iniciar.
 Uso:  powershell -ExecutionPolicy Bypass -File .\scripts\rebuild-index.ps1
#>
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

Write-Host 'Deteniendo users-api...' -ForegroundColor Cyan
docker compose stop users-api
if ($LASTEXITCODE -ne 0) { throw 'No se pudo detener users-api.' }

Write-Host 'Reconstruyendo el indice...' -ForegroundColor Cyan
docker compose run --rm --no-deps users-api --rebuild-index
$codigo = $LASTEXITCODE

Write-Host 'Iniciando users-api...' -ForegroundColor Cyan
docker compose start users-api
if ($codigo -ne 0) { throw "La reconstruccion termino con error ($codigo). Revisa la salida de arriba." }
Write-Host 'Indice reconstruido.' -ForegroundColor Green
