param(
    [string]$EngineRoot='E:\UE_5.8',
    [ValidateRange(18,900)][int]$Seconds=600,
    [ValidatePattern('^[\p{L}\p{N}_-]+$')][string]$Label='현재코드',
    [ValidateSet(-1,0,1,3)][int]$Pairs=-1,
    [ValidateSet(0,1,3)][int]$Speed=0,
    [switch]$Trace,
    [switch]$Mission,
    [ValidateSet('play','heroes','preview','cycle')][string]$UI='play'
)
# 실제 렌더 실행이다. 자동 API 시간 도약이나 실제 사람의 조작 증거가 아니다.
$projectRoot=Split-Path $PSScriptRoot -Parent
$projectPath=Join-Path $projectRoot 'OrbitalVanguard.uproject'
$outputRoot=Join-Path $projectRoot "Saved/Validation/IntegratedPerformance/$Label"
if(Test-Path -LiteralPath (Join-Path $outputRoot '실행.log')){
    throw '같은 Label의 기존 측정 증거가 있습니다. 새로운 Label을 지정하세요.'
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$logPath=Join-Path $outputRoot '실행.log'
$arguments=@("`"$projectPath`"",'/Game/Maps/ControlRange','-game','-windowed','-ResX=1920','-ResY=1080','-d3d11','-NoSplash','-unattended','-OVPerf',"-OVPerfSeconds=$Seconds","-OVPerfLabel=$Label","-OVPerfPairs=$Pairs","-OVPerfSpeed=$Speed","`"-ABSLOG=$logPath`"")
if($Mission){$arguments+='-OVPerfMission'}
$arguments+="-OVPerfUI=$UI"
if($Trace){
    # 이 엔진의 초기 Trace 파일 생성은 한글 경로를 잘못 변환했다.
    # 다른 증거 경로는 그대로 보존하고 Trace 대상만 ASCII 경로에 둔다.
    $traceRoot=Join-Path $projectRoot 'Saved/Profiling/Trace'
    New-Item -ItemType Directory -Path $traceRoot -Force | Out-Null
    $tracePath=Join-Path $traceRoot ("OVPerf_{0}.utrace" -f (Get-Date -Format 'yyyyMMdd_HHmmss_fff'))
    $arguments+=@('-trace=cpu,frame,bookmark',"`"-tracefile=$tracePath`"")
}
$sourceSHA=(git -C $projectRoot rev-parse HEAD).Trim()
$dirty=@(git -C $projectRoot status --porcelain)
$run=[ordered]@{
    '종류'='1920×1080 별도 게임 창의 실제 렌더 장시간 측정'
    '시작시각'=(Get-Date).ToString('o')
    '코드SHA'=$sourceSHA
    '화면조건'=$UI
    '미커밋변경'=$dirty
    '명령'=$arguments
    '직접입력'=$(if($Mission){'없음. 실제 통합 임무의 정상 명령 AI를 1배속 렌더 시계에서 실행함'}else{'없음. 진단기가 출격/교전/귀환을 제어함'})
    '논리코어수'=[Environment]::ProcessorCount
    'GPU'=@(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name)
    'CPU추적경로'=$(if($Trace){$tracePath}else{$null})
    '종료코드'=$null
    '시간초과'=$false
    '프로세스샘플'=[System.Collections.Generic.List[object]]::new()
}
$process=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList $arguments -WindowStyle Hidden -PassThru
$run['PID']=$process.Id
$clock=[System.Diagnostics.Stopwatch]::StartNew()
$lastAt=0.0
$lastCPU=0.0
$logicalCores=[Environment]::ProcessorCount
while(-not $process.HasExited -and $clock.Elapsed.TotalSeconds -lt $Seconds+180){
    Start-Sleep -Seconds 2
    $process.Refresh()
    if($process.HasExited){break}
    $at=$clock.Elapsed.TotalSeconds
    $cpu=$process.TotalProcessorTime.TotalSeconds
    $gpu=Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -ErrorAction SilentlyContinue |
        Where-Object {$_.Name -match "pid_$($process.Id)_" -and $_.Name -match 'engtype_3D'} |
        Measure-Object -Property UtilizationPercentage -Maximum
    $run['프로세스샘플'].Add([ordered]@{
        '벽시계초'=$at
        '전체코어대비CPU백분율'=100*($cpu-$lastCPU)/($at-$lastAt)/$logicalCores
        '프로세스GPU3D최대백분율'=$gpu.Maximum
        '작업집합MB'=$process.WorkingSet64/1MB
        '개인메모리MB'=$process.PrivateMemorySize64/1MB
        '핸들수'=$process.HandleCount
    })
    $lastAt=$at
    $lastCPU=$cpu
    if($run['프로세스샘플'].Count%10 -eq 0){
        $run | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot '프로세스측정.json') -Encoding utf8
        Write-Output ("실제 렌더 실행 중: {0:N0}초 / 요청 {1}초" -f $at,$Seconds)
    }
}
if(-not $process.HasExited){
    # 이 스크립트에서 생성한 PID만 종료한다. 다른 사용자 에디터에는 접근하지 않는다.
    $run['시간초과']=$true
    Stop-Process -Id $process.Id
    $process.WaitForExit()
}
$run['종료코드']=$process.ExitCode
$run['전체벽시계초']=$clock.Elapsed.TotalSeconds
$run['종료시각']=(Get-Date).ToString('o')
$run | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot '프로세스측정.json') -Encoding utf8
python (Join-Path $PSScriptRoot 'MeasureIntegratedPerformance.py') $outputRoot
Write-Output "성능 결과: $outputRoot"
if($run['시간초과'] -or $process.ExitCode -ne 0){throw '진단 프로세스가 정상 완료되지 않았습니다. 로그와 부분 결과를 확인하세요.'}
