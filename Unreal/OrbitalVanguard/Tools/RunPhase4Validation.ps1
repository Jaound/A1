param([string]$EngineRoot='E:\UE_5.8',[string]$Label='Phase4Final',[switch]$Build,[switch]$Automation,[switch]$FullMission,[switch]$Capture)
$projectRoot=Split-Path $PSScriptRoot -Parent
$project=Join-Path $projectRoot 'OrbitalVanguard.uproject'
$logs=Join-Path $projectRoot 'Saved/Logs'
if($Build){
 & (Join-Path $PSScriptRoot 'Build.ps1') -EngineRoot $EngineRoot *> (Join-Path $logs ($Label+'Build.log'))
 if($LASTEXITCODE -ne 0){Get-Content (Join-Path $logs ($Label+'Build.log')) -Tail 25;exit $LASTEXITCODE}
 Write-Output "$Label 빌드 성공"
}
if($Automation){
 & (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $project -unattended -nullrhi -nosplash '-ExecCmds=Automation RunTests OrbitalVanguard' '-TestExit=Automation Test Queue Empty' "-abslog=$(Join-Path $logs ($Label+'Automation.log'))" *> (Join-Path $logs ($Label+'AutomationConsole.log'))
 $auto=Get-Content (Join-Path $logs ($Label+'Automation.log'))
 $passed=@($auto | Select-String -SimpleMatch 'Result={Success}').Count
 $failed=@($auto | Select-String -SimpleMatch 'Result={Fail}').Count
 Write-Output "자동 검사 성공=$passed 실패=$failed"
}
$validators=@('ValidateDevelopmentRepeat.py','ValidateDevelopmentFilters.py','ValidateDevelopment.py','ValidateHullRoles.py','ValidateHeroRoster.py','ValidateHeroBoarding.py','ValidateOwnedMission.py','ValidatePhase2A.py','ValidateD116Calibration.py','ValidatePhase2C.py','ValidateInterior.py')
if($Capture){$validators=@('CaptureDevelopmentUI.py')+$validators}
foreach($validator in $validators){
 $env:OV_VALIDATOR=$validator
 $env:OV_UI_CAPTURE_RES='960x600'
 $env:OV_CAPTURE_FLOATING='1'
 $stem=[IO.Path]::GetFileNameWithoutExtension($validator)
 $arguments=@("`"$project`"",'/Game/Maps/ControlRange','-unattended','-d3d11',"`"-ExecCmds=py $PSScriptRoot/RunValidation.py`"","`"-abslog=$(Join-Path $logs ($Label+$stem+'.log'))`"")
 $process=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
 Write-Output "$stem 종료=$($process.ExitCode) (성공 여부는 개별 JSON 확인)"
}
if($FullMission){
 $env:OV_MISSION_CASES='full'
 $arguments=@("`"$project`"",'/Game/Maps/ControlRange','-unattended','-d3d11',"`"-ExecCmds=py $PSScriptRoot/ValidateIntegratedMission.py`"","`"-abslog=$(Join-Path $logs ($Label+'Mission.log'))`"")
 $process=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -Wait -PassThru -WindowStyle Hidden
 Write-Output "통합 임무 종료=$($process.ExitCode) (완료/성공 필드 별도 확인)"
}
