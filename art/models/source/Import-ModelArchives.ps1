# Copies original ZIPs without unpacking or importing them into Unreal.
# Run from the asset checkout after: git lfs install --local
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$inputRoot = (Resolve-Path -LiteralPath $SourceDirectory).Path
$manifestPath = Join-Path $PSScriptRoot 'manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json
$copied = 0
$missing = @()
foreach ($asset in $manifest.assets) {
    $source = Join-Path $inputRoot $asset.original_filename
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        $missing += $asset.original_filename
        continue
    }
    $destination = Join-Path $repoRoot $asset.destination
    $sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    if (Test-Path -LiteralPath $destination) {
        $existingHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($existingHash -ne $sourceHash) {
            throw "Destination differs; preserve and review before replacing: $destination"
        }
    } else {
        New-Item -ItemType Directory -Path (Split-Path $destination -Parent) -Force | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination
    }
    $destinationHash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($destinationHash -ne $sourceHash) { throw "Copy hash mismatch: $destination" }
    $asset.size_bytes = (Get-Item -LiteralPath $destination).Length
    $asset.sha256 = $destinationHash
    $asset.source_status = 'imported'
    # Mesh/texture inspection and ZIP CRC checks are separate work.
    $copied++
}
$remaining = @($manifest.assets | Where-Object { $_.source_status -ne 'imported' }).Count
if ($remaining -eq 0) {
    $manifest.status = 'source_files_imported'
    $manifest.blocked_reason = $null
} elseif ($remaining -lt $manifest.assets.Count) {
    $manifest.status = 'source_files_partially_imported'
    $manifest.blocked_reason = "Cloud transfer was blocked; $remaining source archives remain pending."
}
$json = $manifest | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText($manifestPath, $json + [Environment]::NewLine, [System.Text.UTF8Encoding]::new($false))
Write-Host "Copied or verified: $copied; pending records: $remaining"
if ($missing.Count -gt 0) {
    Write-Host 'Files absent from the selected source directory:'
    $missing | ForEach-Object { Write-Host "  $_" }
}
Write-Host 'No commit or push performed. Review git status and git lfs ls-files before committing.'

