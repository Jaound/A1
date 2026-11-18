param(
    [Parameter(Mandatory = $true)][string]$Id,
    [Parameter(Mandatory = $true)][string]$RunId,
    [string]$Engine = 'E:\UE_5.8',
    [switch]$CopyToProject
)
$ErrorActionPreference = 'Stop'
if ($Id -notmatch '^[A-Za-z0-9_]+$' -or $RunId -notmatch '^[A-Za-z0-9_]+$') { throw '잘못된 모델 또는 실행 ID' }
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sandbox = Join-Path $root "Saved\Validation\WeaponPitchSandbox_${Id}_${RunId}"
if (Test-Path -LiteralPath $sandbox) { throw '기존 시험 폴더를 덮어쓰지 않습니다.' }
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editor)) { throw '언리얼 에디터가 없습니다.' }
$materialDir = Join-Path $sandbox "Content\Presentation\WeaponsFull\$Id"
[void](New-Item -ItemType Directory -Path $materialDir -Force)
foreach ($file in @('M_Source.uasset','T_Base.uasset','T_Normal.uasset','T_Metallic.uasset','T_Roughness.uasset')) {
    Copy-Item -LiteralPath (Join-Path $root "Content\Presentation\WeaponsFull\$Id\$file") -Destination (Join-Path $materialDir $file)
}
$baseSource = Join-Path $root "Content\Presentation\WeaponsYawTrial\$Id\SM_Base.uasset"
$baseDir = Join-Path $sandbox "Content\Presentation\WeaponsYawTrial\$Id"
[void](New-Item -ItemType Directory -Path $baseDir -Force)
Copy-Item -LiteralPath $baseSource -Destination (Join-Path $baseDir 'SM_Base.uasset')
$project = Join-Path $sandbox 'WeaponPitchSandbox.uproject'
@'
{"FileVersion":3,"EngineAssociation":"5.8","Category":"Tests","Plugins":[{"Name":"PythonScriptPlugin","Enabled":true,"TargetAllowList":["Editor"]},{"Name":"EditorScriptingUtilities","Enabled":true,"TargetAllowList":["Editor"]},{"Name":"AndroidFileServer","Enabled":false}]}
'@ | Set-Content -LiteralPath $project -Encoding utf8
function Run-Editor([string]$Tool, [string]$Log) {
    $script = Join-Path $PSScriptRoot $Tool
    $arguments = @("`"$project`"", '-unattended', '-nullrhi', '-nosplash', "-ExecCmds=`"py $script`"", "-abslog=`"$Log`"")
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "$Tool 에디터 종료 코드: $($process.ExitCode)" }
}
$env:OV_WEAPON_PITCH_ID = $Id; $env:OV_WEAPON_PITCH_RUN = $RunId; $env:OV_WEAPON_PITCH_SOURCE_VERSION = 'v2'
Run-Editor 'ImportWeaponPitchCandidate.py' (Join-Path $sandbox 'Import.log')
$import = Get-Content -LiteralPath (Join-Path $sandbox "Saved\Validation\WeaponPitchImport_${Id}_${RunId}.json") -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not $import.passed) { throw '반입 실패. 시험 폴더와 보고서를 보존합니다.' }
$env:OV_WEAPON_YAW_IDS = $Id; $env:OV_WEAPON_YAW_VALIDATE_RUN = $RunId; $env:OV_WEAPON_PARTS = 'pitch'
Run-Editor 'ValidateWeaponYawCandidatesUE.py' (Join-Path $sandbox 'Reload.log')
$reload = Get-Content -LiteralPath (Join-Path $sandbox "Saved\Validation\WeaponPitchReload_${RunId}.json") -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not $reload.passed) { throw '재로드 실패. 시험 폴더와 보고서를 보존합니다.' }
Run-Editor 'InspectWeaponPitchBoundsUE.py' (Join-Path $sandbox 'Bounds.log')
$bounds = Get-Content -LiteralPath (Join-Path $sandbox "Saved\Validation\WeaponPitchBounds_${Id}_${RunId}.json") -Raw -Encoding UTF8 | ConvertFrom-Json
if (-not $bounds.passed) { throw '경계 측정 실패. 시험 폴더와 보고서를 보존합니다.' }
if ($CopyToProject) {
    $source = Join-Path $sandbox "Content\Presentation\WeaponsPitchTrial\$Id"
    $destination = Join-Path $root "Content\Presentation\WeaponsPitchTrial\$Id"
    if (Test-Path -LiteralPath $destination) { throw '기존 프로젝트 자산을 덮어쓰지 않습니다.' }
    $files = @('SM_Body.uasset','SM_Barrel.uasset','SM_TrialNeck.uasset')
    foreach ($file in $files) { if (-not (Test-Path -LiteralPath (Join-Path $source $file))) { throw "누락된 자산: $file" } }
    [void](New-Item -ItemType Directory -Path $destination -Force)
    foreach ($file in $files) {
        Copy-Item -LiteralPath (Join-Path $source $file) -Destination (Join-Path $destination $file)
        if ((Get-FileHash -LiteralPath (Join-Path $source $file)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $destination $file)).Hash) { throw "해시 불일치: $file" }
    }
}
Write-Output "$Id 반입·재로드·경계 검사 완료. 전장 적용은 별도 검증이 필요합니다."
