# 영웅 시험장 직접 편성 검증

2026-11-24. 시작 코드 `e64a8a9`, 검증 코드 `a5f6f4b4e01bace94688e89ab0e39d0795feb67e`, 기획 `6fb4ae948ff5a828047b94963d9cf66a05b588f8`. 대상은 Windows 개발 빌드, ControlRange의 영웅 시험장이다.

| 검사 | 결과 |
| --- | --- |
| Win64 Development Editor 및 Win64 Development 게임 패키지 | 빌드 성공. DX11 실행본을 별도 폴더에 생성 |
| 전체 C++ 자동 검사 | [165개 통과](Automation.json). 경고 없는 성공 115개, 경고가 있는 성공 50개, 실패·미실행 0개. 실제 포드에서 요격된 영웅의 개인 구조 기록과 진입 전 손실 단계도 확인 |
| DX11 에디터 플레이 | [시험장 기존 검사 54개](HeroArena.json), [직접 편성·혼성 강습 검사 63개](BoardingWaves.json) 모두 통과 |
| Windows 패키지 마우스 입력 | 1440×810에서 팀 편성 창을 열고 보병 1분대를 A팀에서 예비로 이동했다. A팀 8→4명, 예비 8→12명을 확인한 뒤 분대를 복귀시켰다. 이리스 베인을 한 명 생성해 예비에서 A팀으로 옮기고 A팀 영웅 1명·병력 8명 표시를 확인했다. |
| 직접 입력 범위 | 선내 지도 진입층과 결산은 에디터 플레이 API에서 확인했다. 패키지에서 마우스로 임무 끝까지 진행하는 검사는 수행하지 않았다. |

## 실제 수송 결과

각 경로에서 A팀·B팀이 12명씩 진입했다. 경로마다 팀별 알림은 두 줄이며 각각 12명이다. 표의 손실은 한 번의 철수 시험에서 실제 발생한 인원이다. 포드 경로에서는 사전에 고른 영웅과 4인 분대를 교체한 뒤 개별 승객 ID까지 대조했다.

| 수송 | 진입 전 전사 A/B | 선내 전사 A/B | 철수 중 전사 A/B | 실제 모함 귀환 |
| --- | --- | --- | --- | ---: |
| 개인 포드 | 0/0 | 0/0 | 0/0 | 24 |
| 강습선 | 0/0 | 9/0 | 0/0 | 15 |
| 혼합 | 0/0 | 0/1 | 0/8 | 15 |

세 경로 모두 구조 대기 0명이었다. 사망자는 개인 ID로 실제 운송 상태와 알림을 대조했고, 철수 명령 직전의 사망자 집합과 최종 손실을 비교했다. 두 팀의 선내 지도 버튼이 각각 실제 진입층을 가리키는지도 검사했다. 포드 요격으로 구조 대기가 발생하는 별도 사례는 C++ 자동 검사에 포함된다.

## 재현

프로젝트 폴더는 `C:\Users\user\source\repos\A1\Unreal\OrbitalVanguard`이다. 아래 PowerShell 명령은 이 폴더에서 실행한다. 두 PIE 스크립트는 각각 별도 에디터 프로세스에서 완료 후 종료된다.

```powershell
$engine = 'E:\UE_5.8\Engine'
$project = (Resolve-Path .\OrbitalVanguard.uproject).Path
$editor = "$engine\Binaries\Win64\UnrealEditor-Cmd.exe"
& "$engine\Build\BatchFiles\Build.bat" OrbitalVanguardEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
& $editor $project -unattended -nop4 -nosound -nullrhi '-ExecCmds=Automation RunTests OrbitalVanguard' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$PWD\Saved\Validation\BoardingWaveAuto"
foreach ($name in 'ValidateHeroArena','ValidateBoardingWaves') {
    & $editor $project /Game/Maps/ControlRange -unattended -nosplash -d3d11 "-ExecCmds=py $PWD/Tools/$name.py" | Out-Null
}
```

실제 플레이는 [영웅 시험장 안내](../../HERO_ARENA_GUIDE.md)를 따른다. 혼합 수송을 고르고 두 팀을 출발시킨 뒤, 각 팀의 지휘 화면에서 진입층을 확인하고 철수시키면 결산에서 두 팀을 비교할 수 있다. 로컬 빌드·패키지 로그는 프로젝트의 `Saved/Logs/BoardingWavesBuild.log`와 `Saved/Logs/BoardingWavesPackage.log`에 있다.
