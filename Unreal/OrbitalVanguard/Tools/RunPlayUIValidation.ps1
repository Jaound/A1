param([string]$Label='PlayUI',[string[]]$Validators=@('ValidateHeroCards.py','ValidateWeaponPresentation.py','ValidateCompactDevelopment.py','CapturePlayUI.py'),[string]$Resolution='1280x720')
$projectRoot=Split-Path $PSScriptRoot -Parent
$project=Join-Path $projectRoot 'OrbitalVanguard.uproject'
$engine='E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
foreach($validator in $Validators){
 $env:OV_VALIDATOR=$validator
 $env:OV_UI_CAPTURE_RES=$Resolution
 $env:OV_CAPTURE_FLOATING='1'
 $stem=[IO.Path]::GetFileNameWithoutExtension($validator)
 $log=Join-Path $projectRoot ('Saved/Logs/'+$Label+'_'+$stem+'.log')
 $arguments=@("`"$project`"",'/Game/Maps/ControlRange','-unattended','-d3d11',"`"-ExecCmds=py $PSScriptRoot/RunValidation.py`"","`"-abslog=$log`"")
 $process=Start-Process $engine -ArgumentList $arguments -WindowStyle Hidden -PassThru -Wait
 Write-Output "$stem 종료=$($process.ExitCode). 실제 통과는 결과 JSON에서 확인."
}
