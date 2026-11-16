param(
    [ValidateSet('Attack','Defense','FleetOnly')][string]$Mode='Attack',
    [string]$EngineRoot='E:\UE_5.8'
)
# 사용자가 직접 조작할 별도 플레이 창. 설치·전역 설정·Content 변경 없음.
$projectRoot=Split-Path $PSScriptRoot -Parent
$projectFile=Join-Path $projectRoot 'OrbitalVanguard.uproject'
$modeNumber=@{Attack=0;Defense=1;FleetOnly=2}[$Mode]
Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList @(
    "`"$projectFile`"",'/Game/Maps/ControlRange','-game','-windowed','-ResX=1920','-ResY=1080',
    '-d3d11','-NoSplash',"-OVMission=$modeNumber"
) -WindowStyle Normal
