param(
    [Parameter(Mandatory)] [string]$Game,
    [Parameter(Mandatory)] [string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$gamePath = (Resolve-Path -LiteralPath $Game).Path
$folder = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName
foreach ($width in 1280,1920) {
    $height = if ($width -eq 1280) {720} else {1080}
    foreach ($scale in '0.8','1.0','1.5') {
        $stem = "Fleet-${width}x${height}-$scale"
        $argsList = @('-OVPlayScene=Fleet50','-d3d11','-customconfig=HeroArtDX11','-windowed','-forceres','-nosound',
            "-ResX=$width","-ResY=$height","-OVCaptureUIScale=$scale",('-OVCaptureTitleScreen="{0}"' -f (Join-Path $folder "$stem.png")),
            ('-abslog="{0}"' -f (Join-Path $folder "$stem.log")))
        $start = Get-Date
        $process = Start-Process -FilePath $gamePath -ArgumentList $argsList -PassThru
        if (!$process.WaitForExit(90000)) { throw "$stem 캡처 시간 초과" }
        $file = Get-Item -LiteralPath (Join-Path $folder "$stem.png")
        if ($file.Length -eq 0 -or $file.LastWriteTime -lt $start.AddSeconds(-1)) { throw "$stem 새 캡처 없음" }
        $expected = "OV capture: ${width}x${height}, UI " + ([double]::Parse($scale,[cultureinfo]::InvariantCulture)).ToString('F2',[cultureinfo]::InvariantCulture)
        if (!(Select-String -LiteralPath (Join-Path $folder "$stem.log") -SimpleMatch $expected -Quiet)) { throw "$stem 실제 뷰포트·배율 불일치" }
        Write-Output "$stem 통과"
    }
}
