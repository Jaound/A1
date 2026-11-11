param([string]$EngineRoot = 'E:\UE_5.8')
$Project = Join-Path (Split-Path $PSScriptRoot -Parent) 'OrbitalVanguard.uproject'
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') OrbitalVanguardEditor Win64 Development "-Project=$Project" -WaitMutex -NoHotReloadFromIDE
exit $LASTEXITCODE
