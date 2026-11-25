[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$VCRedistRoot
)
$ErrorActionPreference = 'Stop'
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'OrbitalVanguard.uproject'
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if ((Test-Path -LiteralPath $outputRoot) -and (Get-ChildItem -Force -LiteralPath $outputRoot)) {
    throw "새 배포 폴더를 지정하세요. $outputRoot"
}
$null = New-Item -ItemType Directory -Force -Path $outputRoot
$appLocal = Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/AppLocalDependencies'
$batch = Join-Path $EngineRoot 'Engine\Build\BatchFiles'
& (Join-Path $batch 'Build.bat') OrbitalVanguardEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
if ($LASTEXITCODE -ne 0) { throw "Editor build failed ($LASTEXITCODE)" }
& (Join-Path $batch 'RunUAT.bat') BuildCookRun "-project=$project" -target=OrbitalVanguard -platform=Win64 -clientconfig=Shipping -installed -noP4 -build -nocompileeditor -skipbuildeditor -cook -map=ControlRange -AdditionalCookerOptions=-CustomConfig=WindowsRelease -pak -iostore "-applocaldirectory=$appLocal" -prereqs -nodebuginfo -stage -archive "-archivedirectory=$outputRoot" -utf8output -unattended
if ($LASTEXITCODE -ne 0) { throw "Shipping BuildCookRun failed ($LASTEXITCODE)" }
$windows = Join-Path $outputRoot 'Windows'
$required = @(
    'OrbitalVanguard.exe',
    'OrbitalVanguard/Binaries/Win64/OrbitalVanguard-Win64-Shipping.exe',
    'OrbitalVanguard/Content/Paks/OrbitalVanguard-Windows.pak',
    'OrbitalVanguard/Content/Paks/OrbitalVanguard-Windows.utoc',
    'OrbitalVanguard/Content/Paks/OrbitalVanguard-Windows.ucas',
    'OrbitalVanguard/Content/Paks/global.utoc',
    'OrbitalVanguard/Binaries/Win64/msvcp140.dll',
    'Engine/Extras/Redist/en-us/vc_redist.x64.exe'
)
foreach ($file in $required) {
    if (!(Test-Path -LiteralPath (Join-Path $windows $file) -PathType Leaf)) { throw "Missing runtime file: $file" }
}
# Newer MSVC toolsets can require newer CRT exports than the engine bundle.
if ($VCRedistRoot) {
    $crt = @(Get-ChildItem -Directory -LiteralPath (Join-Path $VCRedistRoot 'x64') | Where-Object Name -like 'Microsoft.VC*.CRT')
    if ($crt.Count -ne 1) { throw "Expected one x64 CRT redistributable in $VCRedistRoot" }
    $installer = Get-Item -LiteralPath (Join-Path $VCRedistRoot 'vc_redist.x64.exe')
    if ((Get-AuthenticodeSignature $installer.FullName).Status -ne 'Valid') { throw 'Invalid Microsoft runtime installer signature' }
    foreach ($bin in 'OrbitalVanguard/Binaries/Win64','Engine/Binaries/Win64') {
        foreach ($dll in Get-ChildItem -LiteralPath $crt[0].FullName -Filter '*.dll') {
            $destination = Join-Path (Join-Path $windows $bin) $dll.Name
            Copy-Item -LiteralPath $dll.FullName -Destination $destination -Force
        }
    }
    Copy-Item -LiteralPath $installer.FullName -Destination (Join-Path $windows 'Engine/Extras/Redist/en-us/vc_redist.x64.exe') -Force
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'WindowsRelease-Readme.txt') -Destination (Join-Path $windows '시작 안내.txt')
# IoStore already compresses content. ZIP64 supports containers above 4 GiB.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = Join-Path $outputRoot 'OrbitalVanguard-Windows-x64.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($windows, $zip, [IO.Compression.CompressionLevel]::NoCompression, $false)
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($zip+'.sha256', "$hash  $([IO.Path]::GetFileName($zip))`n", [Text.Encoding]::ASCII)
Write-Output "$zip  SHA256 $hash"
