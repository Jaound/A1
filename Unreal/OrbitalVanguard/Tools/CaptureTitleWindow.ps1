Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms

$win32 = @"
using System;
using System.Runtime.InteropServices;

public class Win32 {
    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT {
        public int X;
        public int Y;
    }
}
"@
Add-Type -TypeDefinition $win32

function Capture-UnrealWindow {
    param (
        [string]$ExtraArgs,
        [string]$OutputPath
    )

    Write-Host "Launching Unreal with: $ExtraArgs"
    $uproject = "C:\Users\user\source\repos\A1\Unreal\OrbitalVanguard\OrbitalVanguard.uproject"
    $editor = "E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
    $argsList = "`"$uproject`" -game -windowed -ResX=1280 -ResY=720 $ExtraArgs"

    $proc = Start-Process -FilePath $editor -ArgumentList $argsList -PassThru
    Write-Host "Process started with PID $($proc.Id). Waiting for window..."

    $hWnd = [IntPtr]::Zero
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.ElapsedMilliseconds -lt 30000) {
        $p = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
        if ($null -ne $p -and $p.MainWindowHandle -ne [IntPtr]::Zero) {
            $hWnd = $p.MainWindowHandle
            break
        }
        Start-Sleep -Milliseconds 500
    }

    if ($hWnd -eq [IntPtr]::Zero) {
        Write-Error "Failed to find window handle within 30 seconds."
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        return $false
    }

    Write-Host "Window found: $hWnd. Giving 6 seconds for 3D stage and UI to render..."
    [Win32]::ShowWindow($hWnd, 9) # SW_RESTORE
    [Win32]::SetForegroundWindow($hWnd)
    Start-Sleep -Seconds 6

    $rect = New-Object Win32+RECT
    [Win32]::GetClientRect($hWnd, [ref]$rect)
    $pt = New-Object Win32+POINT
    $pt.X = $rect.Left
    $pt.Y = $rect.Top
    [Win32]::ClientToScreen($hWnd, [ref]$pt)

    $width = $rect.Right - $rect.Left
    $height = $rect.Bottom - $rect.Top
    Write-Host "Capturing client area: ($($pt.X), $($pt.Y), $width x $height)"

    if ($width -le 0 -or $height -le 0) {
        $width = 1280
        $height = 720
    }

    $bmp = New-Object System.Drawing.Bitmap($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($bmp)
    $graphics.CopyFromScreen($pt.X, $pt.Y, 0, 0, (New-Object System.Drawing.Size($width, $height)), [System.Drawing.CopyPixelOperation]::SourceCopy)

    $dir = [System.IO.Path]::GetDirectoryName($OutputPath)
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    $bmp.Save($OutputPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bmp.Dispose()

    Write-Host "Saved screenshot to: $OutputPath"

    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 1
    return $true
}

$artifactDir = "C:\Users\user\.gemini\antigravity\brain\0da5275e-6784-4e30-ab80-f739659cc0ff"

# 1. Screen 01: Title Screen
Capture-UnrealWindow -ExtraArgs "-OVTitleScreen" -OutputPath "$artifactDir\01_TitleScreen_Actual.png"

# 2. Screen 02: Mission Select Screen
Capture-UnrealWindow -ExtraArgs "-OVTitleScreen -OVTitlePanel=ScenarioSelect" -OutputPath "$artifactDir\02_MissionSelect_Actual.png"

Write-Host "All captures complete!"
