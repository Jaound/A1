param([string]$EngineRoot='E:\UE_5.8',[ValidateRange(0,5)][int]$Scene=5,[int]$Width=1280,[int]$Height=800)
$projectPath=Join-Path (Split-Path $PSScriptRoot -Parent) 'OrbitalVanguard.uproject'
# Uses the already built Editor target in local standalone game mode; not a packaged release.
Start-Process (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe') -ArgumentList @("`"$projectPath`"",'/Game/Maps/ControlRange','-game','-windowed',"-ResX=$Width","-ResY=$Height","-OV2CScene=$Scene",'-NoSplash')
