param([string]$OutputFolder=(Join-Path $env:USERPROFILE 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_PC14_Textures1024'))
$ErrorActionPreference='Stop'
$inputFolder=Join-Path $env:USERPROFILE 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_v1_20261117b/corvette_pc14/Textures'
if(Test-Path -LiteralPath $OutputFolder){throw "Output already exists; no files overwritten: $OutputFolder"}
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Path $OutputFolder | Out-Null
$files=@('Image_0.png','Image_2.png','texture_0_roughness.png','texture_0_metallic.png')
$records=@()
foreach($file in $files){
    $inputPath=Join-Path $inputFolder $file
    $outputPath=Join-Path $OutputFolder $file
    $hashBefore=(Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash
    $inputImage=[System.Drawing.Bitmap]::new($inputPath)
    try {
        $outputImage=[System.Drawing.Bitmap]::new(1024,1024,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics=[System.Drawing.Graphics]::FromImage($outputImage)
            try {
                $graphics.CompositingMode=[System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.InterpolationMode=[System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode=[System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($inputImage,[System.Drawing.Rectangle]::new(0,0,1024,1024))
            } finally {$graphics.Dispose()}
            $outputImage.Save($outputPath,[System.Drawing.Imaging.ImageFormat]::Png)
        } finally {$outputImage.Dispose()}
        $records+=@{name=$file;source_sha256=$hashBefore;source_size=@($inputImage.Width,$inputImage.Height);trial_sha256=(Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash;trial_size=@(1024,1024)}
    } finally {$inputImage.Dispose()}
    if((Get-FileHash -LiteralPath $inputPath -Algorithm SHA256).Hash -ne $hashBefore){throw "Source changed: $inputPath"}
}
$report=@{scope='1024px preview copies; not final source resolution';source_modified=$false;files=$records}
$report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputFolder 'report.json') -Encoding utf8
Write-Output "Trial texture copies created: $OutputFolder"
