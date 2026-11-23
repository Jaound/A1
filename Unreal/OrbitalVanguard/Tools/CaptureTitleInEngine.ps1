param(
    [string]$Mode = "Title" # "Title" or "ScenarioSelect"
)

$uproject = "C:\Users\user\source\repos\A1\Unreal\OrbitalVanguard\OrbitalVanguard.uproject"
$editor = "E:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$artifactDir = "C:\Users\user\.gemini\antigravity\brain\0da5275e-6784-4e30-ab80-f739659cc0ff"

if ($Mode -eq "ScenarioSelect") {
    $outPath = "$artifactDir\02_MissionSelect_Actual.png"
    $argsList = @(
        $uproject,
        "/Game/Maps/ControlRange",
        "-game",
        "-windowed",
        "-ResX=1280",
        "-ResY=720",
        "-NoSplash",
        "-OVTitleScreen",
        "-OVTitlePanel=ScenarioSelect",
        "-OVCaptureTitleScreen=$outPath"
    )
} else {
    $outPath = "$artifactDir\01_TitleScreen_Actual.png"
    $argsList = @(
        $uproject,
        "/Game/Maps/ControlRange",
        "-game",
        "-windowed",
        "-ResX=1280",
        "-ResY=720",
        "-NoSplash",
        "-OVTitleScreen",
        "-OVCaptureTitleScreen=$outPath"
    )
}

Write-Host "Running Unreal Engine capture for Mode=$Mode -> $outPath"
$proc = Start-Process -FilePath $editor -ArgumentList $argsList -PassThru -Wait
Write-Host "Process exited with code: $($proc.ExitCode)"
