<#
 Deja el sistema listo desde cero: comprueba requisitos, crea .env, construye las imagenes y levanta todo.
 Uso:  powershell -ExecutionPolicy Bypass -File .\scripts\setup.ps1
#>
param([switch]$SoloPreparar)   # -SoloPreparar: solo crea .env, sin construir ni levantar
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)

Write-Host '1/4 Comprobando requisitos' -ForegroundColor Cyan
foreach ($comando in 'docker', 'git') {
    if (-not (Get-Command $comando -ErrorAction SilentlyContinue)) { throw "Falta '$comando' en el PATH." }
}
docker info 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Docker no esta corriendo. Abre Docker Desktop y espera a que diga "Running".' }

Write-Host '2/4 Preparando .env' -ForegroundColor Cyan
if (Test-Path .env) {
    Write-Host '  .env ya existe: no se toca. (Para empezar de cero: borrarlo y volver a correr este script)'
} else {
    Copy-Item .env.example .env
    Write-Host '  .env creado desde .env.example. Para uso real, cambia JWT_SECRET y SERVICE_TOKEN (minimo 32 caracteres).'
}
if ($SoloPreparar) { Write-Host 'Listo (solo preparar).' -ForegroundColor Green; exit 0 }

Write-Host '3/4 Construyendo imagenes (la primera vez tarda bastante)' -ForegroundColor Cyan
docker compose build
if ($LASTEXITCODE -ne 0) { throw 'Fallo docker compose build.' }

Write-Host '4/4 Levantando servicios' -ForegroundColor Cyan
docker compose up -d
if ($LASTEXITCODE -ne 0) { throw 'Fallo docker compose up.' }

# Espera a que Users responda (maximo 60 s).
$listo = $false
for ($i = 0; $i -lt 30 -and -not $listo; $i++) {
    try { Invoke-RestMethod -Uri 'http://localhost:8081/api/v1/health' | Out-Null; $listo = $true }
    catch { Start-Sleep -Seconds 2 }
}
if (-not $listo) { throw 'Users no respondio en 60 s. Revisa: docker compose logs users-api' }

Write-Host 'Listo.' -ForegroundColor Green
Write-Host '  Frontend (si lo levantas con npm run dev): http://localhost:5173'
Write-Host '  Users http://localhost:8081  Metadata http://localhost:8082  Streaming http://localhost:8083'
Write-Host '  Aspire (trazas) http://localhost:18888  Prometheus http://localhost:9090  Grafana http://localhost:3000'
Write-Host '  Siguiente: .\scripts\smoke-test.ps1  y  .\scripts\seed.ps1'
