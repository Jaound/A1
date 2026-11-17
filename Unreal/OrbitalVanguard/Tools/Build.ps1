param([string]$EngineRoot = 'E:\UE_5.8')
$Project = Join-Path (Split-Path $PSScriptRoot -Parent) 'OrbitalVanguard.uproject'
$ovProjectRoot=Split-Path $PSScriptRoot -Parent
$ovRevision=(git -C $ovProjectRoot rev-parse HEAD).Trim()
$ovDirty=@(git -C $ovProjectRoot status --porcelain).Count -gt 0
$ovSourceRoot=Join-Path $ovProjectRoot 'Source'
$ovFileHashes=@(Get-ChildItem -LiteralPath $ovSourceRoot -Recurse -File | Sort-Object FullName | ForEach-Object {
    $_.FullName.Substring($ovSourceRoot.Length).Replace('\','/')+':'+(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
})
$ovHasher=[Security.Cryptography.SHA256]::Create()
$ovDigest=$ovHasher.ComputeHash([Text.Encoding]::UTF8.GetBytes(($ovFileHashes -join "`n")))
$ovSourceHash=([BitConverter]::ToString($ovDigest)).Replace('-','').ToLowerInvariant()
$ovHasher.Dispose()
[ordered]@{code_sha=$ovRevision;source_sha256=$ovSourceHash;dirty=$ovDirty;generated_at=(Get-Date).ToString('o');description='Source 상대 경로:개별 SHA-256 목록의 SHA-256. 생성물이며 검증 성공 표시가 아님.'} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $ovProjectRoot 'Config/BuildIdentity.generated.json') -Encoding utf8
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') OrbitalVanguardEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
exit $LASTEXITCODE
