<#
 Crea usuarios de ejemplo (demo1, demo2, ...) a traves de la API de Users. Sirve para demos y pruebas.
 Si un usuario ya existe, lo salta. Requiere el sistema levantado.
 Uso:  powershell -ExecutionPolicy Bypass -File .\scripts\seed.ps1 -Cantidad 5
#>
param(
    [int]$Cantidad = 5,
    [string]$Clave = 'ClaveDemo12345',
    [string]$UsersUrl = 'http://localhost:8081/api/v1'
)
$ErrorActionPreference = 'Stop'
if ($Cantidad -lt 1) { throw 'Cantidad debe ser al menos 1.' }
if ($Clave.Length -lt 8) { throw 'La clave debe tener al menos 8 caracteres.' }

$creados = @()
for ($i = 1; $i -le $Cantidad; $i++) {
    $nombre = "demo$i"
    $cuerpo = @{ username = $nombre; email = "$nombre@example.test"; password = $Clave } | ConvertTo-Json
    try {
        Invoke-RestMethod -Method Post -Uri "$UsersUrl/users" -ContentType 'application/json' -Body $cuerpo | Out-Null
        $creados += [pscustomobject]@{ Usuario = $nombre; Clave = $Clave; Estado = 'creado' }
    } catch {
        $codigo = 0
        if ($_.Exception.Response) { $codigo = [int]$_.Exception.Response.StatusCode }
        if ($codigo -eq 409) {
            $creados += [pscustomobject]@{ Usuario = $nombre; Clave = '(la que ya tenia)'; Estado = 'ya existia' }
        } else {
            throw "No se pudo crear ${nombre}: $($_.Exception.Message)"
        }
    }
}
$creados | Format-Table -AutoSize
