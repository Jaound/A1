param([string]$EngineRoot = 'E:\UE_5.8')
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Project = Join-Path $ProjectRoot 'OrbitalVanguard.uproject'
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path (Join-Path $ProjectRoot 'Binaries\Win64\UnrealEditor-OrbitalVanguard.dll'))) {
    throw 'Build first: run Tools\Build.ps1 with your EngineRoot.'
}
# Visible editor is the interactive application requested by the user.
Start-Process -FilePath $Editor -ArgumentList @("`"$Project`"", '/Game/Maps/ControlRange')
