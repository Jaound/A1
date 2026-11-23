# 진형과 함재기 보충 검증 · 2026-11-23

개발 브랜치 `codex/phase1-playground`. 시작 `6b0959d`, 이전 UI 보존 `d7bfeaa`, 구현 체크포인트 `d1624c6`, 검증 코드 `2d7079727186a4ca0bfe434d2d9f671008de2a6b`. 기획 브랜치 확인 기준 `6d32a1f9dd96349563e30a51eb4d457a7397a836`과 이번 사용자 교정을 적용했다.

## 실제 교전

같은 초기 조건에서 방공함 수와 난수 시드를 명시했다. 매 전투는 실제 함체·도면 PD 장착점·포각, 시설·인원, 쉴드, 장갑, 코어, 함재기 조준·물리탄·요격 피해, 격납고 생산과 자원 재고를 사용한다. 항모와 방공함은 350m 떨어져 있고 방공함 사이 간격은 90m다. 항모와 방공함의 일반 대함 화력은 이 비교에서 제외했다. 실제 함대 임무에서는 정상 작동한다. PD 자체 수치와 방공함의 기존 사거리 특성 1.5배를 사용한다.

| 시드 | 방공함 | 실제 출격 파동 | 종료 게임초 | 방공함 생존 | 각 선체 잔량 (%) | 손실 기체 |
|---:|---:|---|---:|---:|---|---:|
| 126 | 1 | 40 | 162.8 | 1/1 | 37.86 | 40 |
| 126 | 3 | 80 → 80 → 20 | 311.1 | 3/3 | 52.85 / 57.69 / 57.55 | 180 |
| 126 | 2 | 80 → 80 → 20 | 299.0 | 0/2 | 0.00 / 0.00 | 105 |
| 127 | 1 | 40 | 163.1 | 1/1 | 37.80 | 40 |
| 127 | 3 | 80 → 80 → 20 | 319.0 | 3/3 | 54.33 / 57.67 / 55.24 | 180 |
| 127 | 2 | 80 → 80 | 290.1 | 0/2 | 0.00 / 0.00 | 92 |
| 128 | 1 | 40 | 159.2 | 1/1 | 41.07 | 40 |
| 128 | 3 | 80 → 80 → 20 | 317.5 | 3/3 | 51.66 / 58.06 / 57.69 | 180 |
| 128 | 2 | 80 → 80 | 299.1 | 0/2 | 0.00 / 0.00 | 97 |

잔량은 개별 방공함의 최대 선체 HP 대비 값이다. 쉴드 잔량, 코어 HP, 실제 요격 발사·명중·격추 및 함재기 발사·명중 횟수는 [자동 검사 기록](자동검사.json)에 있다. 기체 손실에는 비행 경로에서의 우발 피격이 포함될 수 있다. 1척 시험은 추가 보충 화물 없이 40대를 상대했다. 2척·3척 시험은 80대와 100대분 화물로 시작했다. 위 결과의 적용 범위는 이 배치와 세 시드다.

## 교정 과정

중심 조준 당시 방공함 1척은 19.5초에 코어 파괴로 완파됐고, 선체 표시값은 97.35%였다. 함체 표면의 서로 다른 외곽 구역으로 조준을 나누자 같은 시드에서 40대를 모두 막았지만 잔여 선체는 4.93%였다. 재집결 때 표적을 중심으로 선회하고, 분산 교전하는 AI 편대에 재정렬 시간을 부여했다. 빠른 목표의 이동 방향도 다음 선회점에 반영했다. 기동 시험값은 기본 1.38초, 분산 시 추가 0.27초다. 무장 자체의 최소 발사 간격은 1.2초다. [중간 측정](조준교정비교.json)

## 검증 경로

- UE 5.8.4 Win64 Development Editor 빌드 성공. [빌드 기록](빌드.txt)
- 프로젝트 C++ 자동 검사 149/149 통과. 시드 127·128은 동일한 밸런스 검사 ID의 추가 실행이다. 집결만 확인하는 검사는 손실 시점을 고정하고, 위 표는 별도의 실교전을 사용한다.
- 함대 임무 PIE API 70개 통과. 50·100·200·300코스트 편성, 실제 발사와 양측 출격, 일시정지·배속·재시작·승패를 포함한다. [함대 임무 결과](함대임무.json)
- 별도 1280×740 플레이 창에서 HUD API 및 렌더 15개 통과. [검사 결과](HUD.json) · [진형 화면](formation.png) · [항모 화면](carrier.png)
- 화물 후보 생성기를 4,096회 재실행해 생성 파일이 동일함을 확인했다. 직접 마우스·키보드 입력과 대규모 프레임 성능은 이번 실행에서 측정하지 않았다.

## 재현

프로젝트 폴더 `Unreal/OrbitalVanguard`에서 PowerShell로 실행한다.

```powershell
$p = (Resolve-Path OrbitalVanguard.uproject).Path
& E:/UE_5.8/Engine/Build/BatchFiles/Build.bat OrbitalVanguardEditor Win64 Development "-Project=$p" -WaitMutex
& E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe $p -unattended -nullrhi -nosound '-ExecCmds=Automation RunTests OrbitalVanguard' '-TestExit=Automation Test Queue Empty'
& E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe $p -unattended -nullrhi -nosound -OVWingSeed=127 '-ExecCmds=Automation RunTests OrbitalVanguard.Carrier.DefenceBalanceTargets' '-TestExit=Automation Test Queue Empty'
& E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe $p /Game/Maps/ControlRange -d3d11 "-ExecCmds=py $PWD/Tools/VerifyWingOrders.py"
& E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe $p /Game/Maps/ControlRange -d3d11 "-ExecCmds=py $PWD/Tools/ValidatePlayScenarioFleets.py"
```

직접 확인은 **200코스트 함대전 → 아군 여러 척 선택 → 진형 → 균형 전열 → 배치 적용**으로 시작한다. 초계함의 전방 위치와 상세 창의 함포 2문·쉴드를 확인한다. 항모를 선택하면 **대기 80/80 · 보충 100대분**이 보인다. 함재기를 출격시킨 뒤 교전 중 결원이 생기면 생산 진행과 자원 차감을 보고, 대기 편대의 출격 시점을 직접 선택할 수 있다.
