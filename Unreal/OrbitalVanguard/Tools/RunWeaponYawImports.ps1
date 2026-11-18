param(
    [Parameter(Mandatory = $true)][string[]]$Ids,
    [Parameter(Mandatory = $true)][string]$RunId,
    [string]$Engine = 'E:\UE_5.8'
)
$ErrorActionPreference = 'Stop'
if ($RunId -notmatch '^[A-Za-z0-9_]+$') { throw '검사 ID는 영문·숫자·밑줄만 허용합니다.' }
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$project = Join-Path $projectRoot 'OrbitalVanguard.uproject'
$script = Join-Path $PSScriptRoot 'ImportWeaponYawCandidate.py'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editor) -or -not (Test-Path -LiteralPath $project)) { throw '엔진 또는 프로젝트가 없습니다.' }
foreach ($ident in $Ids) {
    if ($ident -notmatch '^[A-Za-z0-9_]+$') { throw "잘못된 후보 ID: $ident" }
    $result = Join-Path $projectRoot "Saved\Validation\WeaponYawImport_${ident}_${RunId}.json"
    $log = Join-Path $projectRoot "Saved\Logs\WeaponYawCandidate_${ident}_${RunId}.log"
    if ((Test-Path -LiteralPath $result) -or (Test-Path -LiteralPath $log)) {
        Write-Output "$ident 기존 결과 보존; 재실행하지 않음"
        continue
    }
    $env:OV_WEAPON_YAW_ID = $ident
    $env:OV_WEAPON_YAW_RUN = $RunId
    Write-Output "START $ident"
    $arguments = @("`"$project`"", '-unattended', '-nullrhi', '-nosplash', "-ExecCmds=`"py $script`"", "-abslog=`"$log`"")
    # Do not wait on unrelated child processes (such as a persistent trace server).
    $process = Start-Process -FilePath $editor -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $process.WaitForExit()
    if (Test-Path -LiteralPath $result) {
        $record = Get-Content -LiteralPath $result -Raw -Encoding UTF8 | ConvertFrom-Json
        Write-Output "$ident exit=$($process.ExitCode) passed=$($record.passed) parts=$($record.parts.Count)"
    } else {
        Write-Output "$ident exit=$($process.ExitCode) 결과 파일 없음"
    }
}
