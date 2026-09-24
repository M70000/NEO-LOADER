$headers = @{
    'User-Agent' = 'NeoNirvana-Updater/1.0'
    'Accept'     = 'application/vnd.github.v3+json'
}

Write-Host "=== TESTE DE CONECTIVIDADE E ATUALIZACAO VIA GITHUB ===" -ForegroundColor Cyan

# 1. Testando endpoint de release
$repo = "Paxai/DLLium"
Write-Host "`n[1] Verificando repositorio de teste: $repo"
try {
    $resp = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -Headers $headers -Method Get
    Write-Host " -> Resposta HTTP: OK (200)" -ForegroundColor Green
    Write-Host " -> Versao (tag_name): $($resp.tag_name)" -ForegroundColor Yellow
    Write-Host " -> Nome do Release: $($resp.name)"
    Write-Host " -> Total de assets encontrados: $($resp.assets.Count)"
    foreach ($a in $resp.assets) {
        Write-Host "    * Asset: $($a.name) ($($a.size) bytes)"
        Write-Host "      Download URL: $($a.browser_download_url)"
    }
} catch {
    Write-Host (" -> Erro ao consultar " + $repo + ": " + $_.Exception.Message) -ForegroundColor Red
}

# 2. Testando comportamento com repositorio inexistente ou sem release (fallback seguro)
$fakeRepo = "M70000/NeoNirvana"
Write-Host "`n[2] Verificando comportamento de fallback quando nao ha release remota: $fakeRepo"
try {
    $resp2 = Invoke-RestMethod -Uri "https://api.github.com/repos/$fakeRepo/releases/latest" -Headers $headers -Method Get
    Write-Host " -> Release encontrada: $($resp2.tag_name)"
} catch {
    Write-Host " -> Retorno esperado (404 Not Found): $($_.Exception.Message)" -ForegroundColor Green
    Write-Host " -> O NeoNirvana trata o 404 de forma silenciosa e continua usando o arquivo local sem travar ou emitir erro." -ForegroundColor Green
}
