param([string]$EngineRoot='E:\UE_5.8')
$projectPath=Join-Path (Split-Path $PSScriptRoot -Parent) 'OrbitalVanguard.uproject'
$savedRoot=Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved'
$scriptPath=Join-Path $PSScriptRoot 'MeasurePhase2C.py'
$logPath=Join-Path $savedRoot 'Logs/Phase2CPerformance.log'
$outPath=Join-Path $savedRoot 'Validation/Phase2CProcessLoad.json'
$process=Start-Process (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe') -ArgumentList @("`"$projectPath`"",'/Game/Maps/ControlRange','-NoSplash','-unattended',"`"-ExecCmds=py $scriptPath`"","`"-ABSLOG=$logPath`"") -WindowStyle Hidden -PassThru
$samples=[System.Collections.Generic.List[object]]::new()
$clock=[System.Diagnostics.Stopwatch]::StartNew()
$lastTime=0.0; $lastCPU=0.0
$logicalCores=[Environment]::ProcessorCount
while(-not $process.HasExited -and $clock.Elapsed.TotalSeconds -lt 90) {
    Start-Sleep -Seconds 2
    $process.Refresh()
    if($process.HasExited) { break }
    $seconds=$clock.Elapsed.TotalSeconds
    $cpu=$process.TotalProcessorTime.TotalSeconds
    $gpuSamples=Get-CimInstance Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match "pid_$($process.Id)_" -and $_.Name -match 'engtype_3D' }
    $gpu=($gpuSamples | Measure-Object -Property UtilizationPercentage -Maximum).Maximum
    $samples.Add([pscustomobject]@{wall_s=$seconds;cpu_percent_machine=100*($cpu-$lastCPU)/($seconds-$lastTime)/$logicalCores;gpu_3d_max_percent=$gpu;working_set_mb=$process.WorkingSet64/1MB})
    $lastTime=$seconds; $lastCPU=$cpu
}
$gpuNames=Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name
@{kind='Read-only OS process counters during rendered Editor benchmark';pid=$process.Id;logical_cores=$logicalCores;gpu_names=$gpuNames;samples=$samples;cpu_note='Process CPU seconds divided by wall seconds and logical core count';gpu_note='Maximum observed per-process 3D engine utilization; null means unavailable; not total system GPU load'} |
    ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $outPath -Encoding utf8
Write-Output $outPath
