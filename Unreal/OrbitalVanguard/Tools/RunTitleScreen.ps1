param(
    [string]$EngineRoot = 'E:\UE_5.8',
    [switch]$Build,
    [int]$Width = 1280,
    [int]$Height = 720
)

$ErrorActionPreference = 'Stop'
$projectPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'OrbitalVanguard.uproject'

if ($Build) {
    $buildBat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    if (-not (Test-Path $buildBat)) {
        throw "Unreal Build.bat not found at: $buildBat"
    }
    Write-Host "[OV] Building OrbitalVanguardEditor Win64 Development..." -ForegroundColor Cyan
    & $buildBat OrbitalVanguardEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReload
    if ($LASTEXITCODE -ne 0) {
        throw "UE 5.8 build failed with exit code $LASTEXITCODE"
    }
}

$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path $editorExe)) {
    throw "UnrealEditor.exe not found at: $editorExe"
}

Write-Host "[OV] Launching Orbital Vanguard Start Screen (SM_Hull backdrop)..." -ForegroundColor Green
Start-Process $editorExe -ArgumentList @(
    $projectPath,
    '/Game/Maps/ControlRange',
    '-game',
    '-windowed',
    "-ResX=$Width",
    "-ResY=$Height",
    '-OVTitleScreen',
    '-NoSplash'
)
