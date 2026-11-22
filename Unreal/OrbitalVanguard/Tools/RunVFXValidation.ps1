param([string]$EngineRoot='E:\UE_5.8')
$projectRoot = Split-Path $PSScriptRoot -Parent
$project = Join-Path $projectRoot 'OrbitalVanguard.uproject'
$logs = Join-Path $projectRoot 'Saved/Logs'
Copy-Item (Join-Path $PSScriptRoot 'ValidateAndCaptureVFX.py') 'C:\Users\user\A1\Unreal\OrbitalVanguard\Tools\ValidateAndCaptureVFX.py' -Force
$env:OV_VALIDATOR = 'ValidateAndCaptureVFX.py'
$env:OV_UI_CAPTURE_RES = '1280x720'
$env:OV_CAPTURE_FLOATING = '1'
$arguments = @(
    "`"$project`"",
    '/Game/Maps/ControlRange',
    '-unattended',
    '-NoLiveCoding',
    '-d3d11',
    "`"-ExecCmds=py $PSScriptRoot/RunValidation.py`"",
    "`"-abslog=$(Join-Path $logs 'VFXShowcaseValidation.log')`""
)
$process = Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -Wait -PassThru -WindowStyle Minimized
Write-Output "ValidateAndCaptureVFX 종료=$($process.ExitCode)"
Copy-Item (Join-Path $PSScriptRoot 'RunVFXValidation.ps1') 'C:\Users\user\A1\Unreal\OrbitalVanguard\Tools\RunVFXValidation.ps1' -Force
exit $process.ExitCode
