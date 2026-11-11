# 3D 조작 시험장 검증 결과

검증일: **2026-11-11 KST**. 대상은 `Unreal/OrbitalVanguard`의 첫 제작 단계이며, 실행·조작 안내는 [PLAYGROUND_GUIDE.md](PLAYGROUND_GUIDE.md)를 참고한다.

**Win64 Development Editor 빌드와 3개 자동 테스트가 성공했다. 실제 에디터에서 ControlRange를 열고 PIE 월드의 동작도 확인했다.** PIE 검사는 엔진 API로 명령을 호출한 통합 검사이며, 물리 마우스·키보드 입력부터 화면 반응까지의 전체 경로를 검증한 것은 아니다.

## 빌드 환경과 결과

| 항목 | 실제 확인 결과 |
| --- | --- |
| 엔진 | UE 5.8.4, CL 58902052, `E:\UE_5.8` |
| 타깃 | `OrbitalVanguardEditor Win64 Development` |
| 결과 | UnrealBuildTool `Result: Succeeded`, 종료 코드 0, 최종 빌드 6.76초 |
| MSVC 도구 버전 | **14.51.36260**, Host x64 / Target x64 |
| MSVC 설치 폴더 | `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC\14.51.36231` |
| 컴파일러 | 위 폴더의 `bin\Hostx64\x64\cl.exe` |
| Windows SDK | **10.0.26100.0**, `C:\Program Files (x86)\Windows Kits\10` |

UBT는 MSVC 14.51.36260이 선호 버전 14.50.35717보다 새롭다는 경고를 남겼다. 이 설치 조합에서 이번 프로젝트의 컴파일·링크가 성공했다는 사실을 확인했으며, 이를 모든 UE 기능에 대한 공식 호환 보증으로 확대하지 않는다. 도구 설치나 전역 설정 변경 없이 기존 설치를 사용했다. 요약 근거: [Build-summary.txt](validation/Build-summary.txt).

## 자동 테스트

`OrbitalVanguard.Ship` 필터로 다음 **3개 테스트 모두 Success**, 자동화 종료 코드 **0**을 확인했다. 최종 코드 빌드 후 다시 실행한 완료 시각은 2026-11-11 **12:52:04 KST**다. 이 실행은 `-nullrhi`를 사용하므로 화면 렌더링이나 운영체제 입력의 검증에 해당하지 않는다.

| 테스트 | 검증 대상 |
| --- | --- |
| `ClassTurnRates` | 체급별 선회 속도 차이 |
| `FiniteTurnXYZArrival` | 제한된 선회, XYZ 목적지 도착, 정지 |
| `TimeScaleAndStoppedArrival` | 시간 배율에 따른 이동과 도착 후 정지 |

요약 근거: [Automation-summary.txt](validation/Automation-summary.txt). 최종 원본 로그는 로컬 프로젝트 `Saved/Logs/FinalAutomation.log`에 있다.

### 별도로 확인한 엔진 시작 자체 검사 오류

기본 한국어 환경에서는 프로젝트 테스트 실행 전, UE 엔진의 시작 자체 검사에서 조건 실패 **15건**이 기록되었다. 상세 로그를 활성화하여 아래 엔진 테스트로 출처를 확인했고, 같은 실행에 일시적인 명령줄 옵션 `-culture=en`을 적용해 비교했다.

| 엔진 자체 검사 | 기본 한국어 실행 | 영어 옵션 비교 실행 |
| --- | --- | --- |
| `FUnifiedErrorTest_CreateErrorMessage` | 조건 실패 7건 | Success |
| `FUnifiedErrorTest_CreateErrorMessageWithContext` | 조건 실패 4건 | Success |
| `FStructuredLogFormatTest` | 조건 실패 4건 | Success |

이 세 테스트는 엔진 소스의 `Runtime/Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp`와 `Runtime/Core/Tests/Logging/StructuredLogFormatTest.cpp`에 있으며, 지역화된 출력과 고정 영문 문자열의 비교가 포함되어 있다. 비교 실행으로 언어 설정에 따른 실패임을 확인했다. StructuredLog의 정확한 실패 입력 네 개는 로그에 값이 없어 특정하지 않았다. **프로젝트 자동 테스트 3개 및 PIE 검사 20개 성공과 별개로, 한국어 환경의 엔진 자체 검사 오류는 남아 있다.** 엔진 소스·전역 언어 설정은 변경하지 않았으며 일반 실행도 영어로 강제하지 않는다. 근거: [Engine-startup-summary.txt](validation/Engine-startup-summary.txt).

영어 비교는 시작 자체 검사의 결과를 수집한 진단 실행이다. 결과 수집 후 해당 진단 프로세스만 종료했으며, 이 실행의 프로세스 종료 코드를 프로젝트 자동 테스트의 종료 코드 0과 혼동하지 않는다.

## 실제 에디터와 PIE

에디터에서 `/Game/Maps/ControlRange`를 열고 `LevelEditorSubsystem.editor_request_begin_play()`로 실제 PIE를 시작했다. 게임 월드 경로는 `/Game/Maps/UEDPIE_0_ControlRange.ControlRange`였다. [ValidatePIE.py](../Unreal/OrbitalVanguard/Tools/ValidatePIE.py)의 결과는 **20개 검사 성공, errors 빈 배열, passed=true**이다. 보존한 원본 결과: [PIE.json](validation/PIE.json).

| 확인 항목 | 방법과 확인 범위 |
| --- | --- |
| 시험장·함선 생성 | 에디터 맵, 실제 PIE 월드, 아군 2척·적 1척 확인 |
| 일시정지 | 실제 프레임을 기다린 뒤 함선 위치와 시뮬레이션 시간이 유지되는지 확인 |
| 정지 중 선택·카메라 | 컨트롤러 선택 명령과 카메라 기준점·각도·거리 변경 API로 이동·회전·줌 결과 확인 |
| 정지 중 XYZ 명령 | 목적지 확정·수동 전환 확인, 정지 상태에서 시뮬레이션 함수를 호출해도 이동하지 않음 확인 |
| XYZ 이동·도착 정지 | 지정한 시간 간격으로 `StepSimulation`을 호출하여 `(-200, 1200, 1400)` 도착과 정지 확인 |
| 수동·자동 | 도착 후 수동 유지, AUTO 전환 시 표적 방향으로 회전, 명시적 MANUAL 복귀 확인 |
| 1/2/3배속 | 같은 입력 시간에 시뮬레이션 시간 1:2:3, 이동 거리 약 850 / 1700 / 2550 u 확인 |
| 실제 프레임의 이동 | 명령 후 약 2초간 PIE 프레임이 진행되도록 두어 `PlayerTick`에 의한 위치·시뮬레이션 시간 증가 확인 |
| 정지 중 고도 조정 | 이동 입력 상태에서 Up 명령으로 목적지 Z가 250 u 증가하는지 확인 |

`StepSimulation`에 지정한 델타를 넣는 검사는 이동 계산과 배속 비율을 검사한다. 별도 실제 프레임 검사는 PIE의 `PlayerTick` 연결을 확인한다. 카메라 값·목적지 값을 API로 지정한 부분은 키 입력, 마우스 광선의 XY 계산, 휠 전달, HUD 클릭 영역을 통과하지 않는다.

## 화면 확인과 직접 확인하지 못한 조작

Windows 화면 캡처 도구는 `FrameArrived` 대기 시간 초과가 발생했고, 창 캡처 재시도도 시간 초과로 끝났다. 대신 UE 내장 `Shot SHOWUI`로 PIE 이미지를 생성했다. 생성된 화면에서 청록색·파란색 아군과 빨간색 표적, 선택 표시, 목적지 고도 표시, HUD 패널이 렌더링되는 것을 확인했다. 화면 근거: [ControlRange-PIE.png](validation/ControlRange-PIE.png).

**물리 입력을 통한 아래 항목은 미확인**이다. 수동 확인 시 1280×720 이상의 PIE 뷰포트에서 게임 영역을 클릭하여 포커스를 준 뒤 진행한다.

- 함선 왼쪽 클릭 선택, F1/F2 선택, HUD의 선택·MANUAL/AUTO·PAUSE·1x/2x/3x 버튼 클릭.
- WASD/방향키와 Q/E 이동, 가운데 버튼 드래그 회전, 휠 줌, F/Home. 정지 상태에서도 반복.
- R/오른쪽 클릭으로 명령 시작 → 마우스로 XY 지정 → 휠·고도 버튼으로 Z 변경 → 왼쪽 클릭/Enter/CONFIRM 확정. 오른쪽 클릭/Backspace/CANCEL 취소도 확인.
- 실제 Space 및 1/2/3 입력, 정지 중 명령을 입력한 뒤 재개, F9 계측 로그. F1/F2 사용 후 뷰 모드가 바뀌지 않는지도 확인.

## 재현 명령

아래 명령은 현재 로컬 경로 기준이다. 에디터가 열려 있다면 저장할 내용을 저장하고 종료한 뒤 빌드를 실행한다. 다른 설치 위치에서는 엔진·저장소 경로와 마지막 PIE 명령의 스크립트 경로를 바꾼다.

```powershell
$repo = 'C:\Users\user\source\repos\A1'
$engineRoot = 'E:\UE_5.8'
$projectPath = Join-Path $repo 'Unreal\OrbitalVanguard\OrbitalVanguard.uproject'
$editorPath = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
& (Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat') OrbitalVanguardEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE
```

자동 테스트는 빌드 성공 후 실행한다.

```powershell
& $editorPath $projectPath -unattended -nullrhi -nosplash '-ExecCmds=Automation RunTests OrbitalVanguard.Ship;Quit' '-TestExit=Automation Test Queue Empty'
```

PIE 통합 검사는 일반 에디터를 실행하며, 완료 후 PIE가 일시정지된 상태로 남는다. `Saved/Validation/PIE.json`과 `Saved/Screenshots/WindowsEditor`의 캡처를 생성한다.

```powershell
& $editorPath $projectPath /Game/Maps/ControlRange -windowed -ResX=1440 -ResY=900 -NoSplash -NoLiveCoding '-ExecCmds=py C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/ValidatePIE.py'
```

일반 플레이는 [RunEditor.ps1](../Unreal/OrbitalVanguard/Tools/RunEditor.ps1)로 에디터를 열고 Selected Viewport PIE로 시작한다. AUTO는 이번 단계에서 **제자리 표적 방향 추적**만 수행한다. 전투·사격·진형 AI나 다음 제작 단계의 기능은 이 결과에 포함하지 않는다.
