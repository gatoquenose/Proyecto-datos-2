param(
    [string]$UsersUrl = 'http://localhost:8081/api/v1',
    [string]$MetadataUrl = 'http://localhost:8082/api/v1',
    [string]$StreamingUrl = 'http://localhost:8083/api/v1',
    [string]$VideoPath = '',
    [int]$ChunkSize = 4194304
)
$ErrorActionPreference = 'Stop'

function Step($name, [scriptblock]$action) {
    Write-Host "--- $name" -ForegroundColor Cyan
    try { $r = & $action; Write-Host "OK" -ForegroundColor Green; return $r }
    catch {
        Write-Host "FALLA: $($_.Exception.Message)" -ForegroundColor Red
        if ($_.ErrorDetails) { Write-Host $_.ErrorDetails.Message -ForegroundColor Red }
        exit 1
    }
}
function Sha256Hex([byte[]]$bytes) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    return ([BitConverter]::ToString($sha.ComputeHash($bytes)) -replace '-', '').ToLower()
}

Step 'health users' { Invoke-RestMethod -Uri "$UsersUrl/health" | Out-Null }
Step 'health streaming' { Invoke-RestMethod -Uri "$StreamingUrl/health" | Out-Null }
Step 'metadata responde (se espera 401 sin token)' {
    $code = 0
    try { Invoke-WebRequest -Uri "$MetadataUrl/videos" -UseBasicParsing | Out-Null; $code = 200 }
    catch { $code = [int]$_.Exception.Response.StatusCode }
    if ($code -ne 401) { throw "se esperaba 401 y llego $code" }
}

$user = 'smoke' + (Get-Random -Maximum 99999)
$pass = 'ClaveSegura123'
Step 'registro' {
    $b = @{ username = $user; email = "$user@example.test"; password = $pass } | ConvertTo-Json
    Invoke-RestMethod -Method Post -Uri "$UsersUrl/users" -ContentType 'application/json' -Body $b | Out-Null
}
$session = Step 'login' {
    $b = @{ username = $user; password = $pass } | ConvertTo-Json
    Invoke-RestMethod -Method Post -Uri "$UsersUrl/auth/login" -ContentType 'application/json' -Body $b
}
$auth = @{ Authorization = ($session.tokenType + ' ' + $session.accessToken) }

Step 'GET /users/me' { Invoke-RestMethod -Uri "$UsersUrl/users/me" -Headers $auth | Out-Null }
Step 'GET /users/me/quota' { Invoke-RestMethod -Uri "$UsersUrl/users/me/quota" -Headers $auth | Out-Null }

$sizeBytes = 1048576
if ($VideoPath -ne '') { $sizeBytes = (Get-Item $VideoPath).Length }

# Metadata exige exactamente: title, description, author, topic, tags, visibility, sizeBytes
$video = Step 'crear video (metadata)' {
    $payload = [ordered]@{
        title = 'Video de humo'; description = 'smoke'; author = 'smoke'; topic = 'pruebas'
        tags = @('smoke'); visibility = 'private'; sizeBytes = [int64]$sizeBytes
    }
    Invoke-RestMethod -Method Post -Uri "$MetadataUrl/videos" -Headers $auth -ContentType 'application/json' -Body ($payload | ConvertTo-Json)
}
Step 'listar videos' {
    $l = Invoke-RestMethod -Uri "$MetadataUrl/videos?pageSize=5&sortBy=uploadedAt&order=desc" -Headers $auth
    if (-not ($l.items | Where-Object { $_.id -eq $video.id })) { throw 'el video creado no aparece en la lista' }
}

if ($VideoPath -eq '') {
    Write-Host 'Sin -VideoPath: se omite subida y reproduccion.' -ForegroundColor Yellow
    exit 0
}

$file = [System.IO.File]::ReadAllBytes($VideoPath)
$total = $file.Length
$chunks = [int][Math]::Ceiling($total / $ChunkSize)
$fileHash = Sha256Hex $file

# Contrato real de Streaming (decision del equipo): sizeBytes, totalChunks, fileChecksum, contentType
$upload = Step 'startUpload (streaming)' {
    $b = @{ sizeBytes = $total; totalChunks = $chunks; fileChecksum = $fileHash; contentType = 'video/mp4' } | ConvertTo-Json
    Invoke-RestMethod -Method Post -Uri "$StreamingUrl/videos/$($video.id)/upload" -Headers $auth -ContentType 'application/json' -Body $b
}
for ($i = 0; $i -lt $chunks; $i++) {
    $start = $i * $ChunkSize
    $len = [Math]::Min($ChunkSize, $total - $start)
    $part = New-Object byte[] $len
    [Array]::Copy($file, $start, $part, 0, $len)
    Step "fragmento $($i + 1) de $chunks" {
        $h = @{ Authorization = $auth.Authorization; 'Content-Range' = "bytes $start-$($start + $len - 1)/$total"; 'X-Chunk-Checksum' = (Sha256Hex $part) }
        Invoke-WebRequest -Method Put -Uri "$StreamingUrl/uploads/$($upload.uploadId)/chunks/$i" -Headers $h -ContentType 'application/octet-stream' -Body $part -UseBasicParsing | Out-Null
    }
}
Step 'completeUpload' {
    $b = @{ fileChecksum = $fileHash } | ConvertTo-Json
    Invoke-RestMethod -Method Post -Uri "$StreamingUrl/uploads/$($upload.uploadId)/complete" -Headers $auth -ContentType 'application/json' -Body $b | Out-Null
}
Step 'esperar transcodificacion (max 5 min)' {
    for ($t = 0; $t -lt 60; $t++) {
        $p = Invoke-RestMethod -Uri "$StreamingUrl/videos/$($video.id)/processing" -Headers $auth
        Write-Host "  estado: $($p.status)"
        if ($p.status -eq 'failed') { throw 'la transcodificacion fallo' }
        if ($p.status -notin @('active', 'queued', 'uploading', 'processing')) { return }
        Start-Sleep -Seconds 5
    }
    throw 'timeout esperando el fin del procesamiento'
}
Step 'manifiesto DASH' {
    $m = Invoke-WebRequest -Uri "$StreamingUrl/videos/$($video.id)/manifest.mpd" -Headers $auth -UseBasicParsing
    if ($m.Content -notmatch '<MPD') { throw 'la respuesta no es un manifiesto MPD' }
}
Write-Host 'FLUJO COMPLETO OK' -ForegroundColor Green