# 2A 검증 결과

갱신 2026-11-12. 로컬 Windows `C:\Users\user\source\repos\A1`, 작업 브랜치 `codex/phase1-playground`. 기획 기준 `cb96d2c62d272552044d236a4401330e8854aa7d`. 원격 `work`를 다시 조회했으며 이후 변경은 없었다. 기존 미커밋 구현은 `c2f6627`, 부분 헤더는 `4a32446`, 최신 기획은 `274a55e`, 첫 2A 실행 가능 소스는 `873308c`, 자산/보완 구현은 `bc95a68`, 마지막 초광폭 카메라/포커스 보완은 `379414d`에 보존했다. 일반 Git 인증 경로의 제한 때문에 기획 동기화는 인증된 GitHub 연결로 Git 객체를 가져와 원래 SHA를 검증한 뒤 병합했다. 무조건 pull/reset/덮어쓰기는 하지 않았다.

## 판정과 증거

| 검증 | 결과 | 범위와 근거 |
| --- | --- | --- |
| Win64 Development Editor | 성공 | UE 5.8.4 `E:\UE_5.8`, MSVC **14.51.36260** (폴더 14.51.36231), Windows SDK **10.0.26100.0**. [빌드 발췌](validation/Phase2A-Build.txt) |
| 프로젝트 자동 테스트 | **15/15 통과**, 종료 0 | 기존 이동 3개 유지, 입력 2개, 레이아웃 2개, 시스템 8개. [자동 테스트 결과](validation/Phase2A-Automation.txt) |
| 실제 에디터 PIE API | **45/45 통과** | `UEDPIE_0_ControlRange`에서 실행. [체크별 JSON](validation/Phase2A-PIE.json) |
| 레이아웃 자산 | 성공 | UE Python/AssetTools로 직렬화한 1/2/3층 DataAsset 로드·검증, 장갑 14/26/38개. [자산 결과](validation/Phase2A-Layouts.txt) |
| 실제 렌더 화면 | 확인 | 에디터 `Shot SHOWUI` 출력 [PIE 화면](validation/Phase2A-PIE.png). 화면 제어 도구의 캡처와 별개 |
| OS 마우스·키보드 입력 | **미검증** | Windows computer-use 도구에서 `FrameArrived timed out`, 최소화 응답 후 창 복원·재선택에서도 `window capture timed out`. 위치를 추측해 입력하지 않음 |
| 한국어 엔진 자체 검사 | 기존 오류 **15건 별도 유지** | `FUnifiedErrorTest_CreateErrorMessage` 7, `...WithContext` 4, `FStructuredLogFormatTest` 4. 프로젝트 자동 테스트 결과와 합산하지 않음. [별도 발췌](validation/Phase2A-EngineStartup.txt) |

최종 자동 테스트는 2026-11-12 07:44 KST, PIE API와 화면 캡처는 07:45 KST다. 최종 C++ 빌드는 6.27초에 성공했으며 15개 자동 테스트와 45개 PIE API 검사는 최종 소스를 대상으로 한다. 엔진/전역 언어·도구 설치·전역 설정은 변경하지 않았다. 기존 영어 비교 실행의 엔진 자체 검사 정상 기록은 [1단계 검증](PLAYGROUND_VALIDATION.md)에 보존되어 있다.

MSVC는 엔진의 선호 버전 14.50.35717보다 새롭다는 경고가 있다. Visual Studio SDK 부재는 에디터 IDE 통합 경고이며 설치된 Windows SDK 부재를 뜻하지 않는다. 대상이 아닌 Linux/Android 등의 SDK 탐색 안내도 Win64 빌드 실패로 처리하지 않았다.

## 검사 내용과 수정한 문제

- 자동 테스트: 가속·제동·체급 선회·XYZ 도착·같은 게임 시간, 고도 앵커/8px 임계/차단 재진입/불안정 광선, 6면·경계·슬롯 매핑과 잘못된 배치 거부, 독립 상태/중복 효율/점령·인원, 쉴드 소실·회복·50% 재가동·부분 용량 감소, 장갑 예외·사보타주·유폭 1회·빈 슬롯/중층 관통·무장 HP·완파.
- PIE API: 3개 저장 자산을 실제 로드, 일시정지 중 카메라와 명령, XYZ 도착, Pitch/Roll 유지, Auto 전환 후 Hold, 수동 표적 분리, 추적 토글, 1/2/3배속 공통 시간·이동·쉴드 일치, 충돌 쌍 1회·FTL 고정 경계, 회전/롤/비균일 배율 후 월드 피격 매핑, 다중 선택 대표 교체, 장갑/무장 DEV 도구, 층/적 정보 공개, 창 드래그/리사이즈/프리뷰 입력 API, 추가 함선 생성 없음, 완파 명령 차단, 실제 프레임에서 이동·시간 진행.
- 첫 PIE 스크립트의 읽기 전용 `Velocity` 직접 변경은 실패했다. 검사와 DEV 초기화가 명시적인 `DebugResetMotion`/FTL 시험 API를 사용하도록 고쳤다. 관련 Python 프로퍼티 이름 충돌은 C++/직렬화 이름을 유지하고 `ScriptName` 메타데이터로 분리했다.
- PIE에서 실제 입력 차단 결함을 찾았다. 엔진은 렌더 직후 `AHUD::Canvas`를 해제하므로 입력 시 Canvas 존재를 조건으로 삼으면 HUD 차단이 사라졌다. 뷰포트 크기 기반 계산으로 수정했고 렌더 콜백 밖의 포인터 API 검사로 확인했다.
- 초광폭 화면에서 CameraActor의 기본 16:9 고정이 HUD 표시와 입력 좌표를 어긋나게 할 수 있어 전체 뷰포트를 사용하도록 수정했다. 포커스 상실 시 Deproject 단락 평가로 초기화되지 않은 광선을 읽던 C4701 경고도 원인 수정했다. 마지막 프로젝트 C++ 빌드에 해당 경고는 없다.
- 다중 선택 대표가 3층 함선에서 1층 함선으로 바뀔 때 상세 인덱스를 초기화하고 배열 접근을 검사한다. 프리뷰 도형·클릭 영역을 같은 사각형에 제한하고, 리사이즈 손잡이의 오른쪽/아래쪽 범위도 검사한다.

API 호출은 OS 이벤트 전달, 키보드 포커스, 실제 클릭 좌표와 드래그 감각을 검증하지 않는다. 직접 입력 후 남은 확인은 [조작 안내의 8단계 체크리스트](PHASE2A_GUIDE.md#짧은-직접-조작-확인-순서)를 사용한다. 실제 입력이 통과했다고 기록하려면 이 절차를 별도로 실행해야 한다.

## 재실행

PowerShell의 저장소 루트 기준이다. 에디터가 DLL을 사용 중이면 종료한 뒤 빌드한다.

```powershell
$engineRoot = 'E:\UE_5.8'
$projectPath = Join-Path (Get-Location) 'Unreal\OrbitalVanguard\OrbitalVanguard.uproject'
$toolsPath = Join-Path (Get-Location) 'Unreal\OrbitalVanguard\Tools'
& "$engineRoot\Engine\Build\BatchFiles\Build.bat" OrbitalVanguardEditor Win64 Development "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $projectPath -unattended -nullrhi -nosplash '-ExecCmds=Automation RunTests OrbitalVanguard' '-TestExit=Automation Test Queue Empty' '-LogCmds=LogAutomationTest Log'
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor.exe" $projectPath '/Game/Maps/ControlRange' -NoSplash "-ExecCmds=py $toolsPath\ValidatePhase2A.py"
```

PIE 검사는 에디터에서 시작하며 종료 시 정지 상태로 화면을 남긴다. 결과는 `Saved/Validation/Phase2A.json`, 로그는 `Saved/Logs`, 화면은 `Saved/Screenshots/WindowsEditor`에 생성된다. 공간이 있는 다른 체크아웃 경로에서는 에디터 Output Log의 Python 실행으로 스크립트를 선택한다.

자산을 새 체크아웃에서 복원해야 할 경우에만 다음 명령을 사용한다. 기존 자산은 덮어쓰지 않고 보존·검증한다.

```powershell
& "$engineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $projectPath -unattended -nullrhi -run=pythonscript "-script=$toolsPath\CreateTestLayouts.py"
```

## 범위와 남은 결정

2A 상태·피해·입력과 최소 관측 UI의 빌드/로직/API 검증을 완료했다. 실제 마우스·키보드 검증은 남았다. 이번 최소 UI는 Canvas에서 장갑·무장 상태를 읽어 투영하는 3D 도식이며 실시간 전장 카메라·최종 함선 메시·2핀 UI는 아니다. 직접 피해 버튼은 실제 사격/보급수리를 대체한 최종 기능이 아니다.

R11 비전투 완충은 임시 Gradual 프로필, 수동 쉴드 전환은 기본 비활성이다. R14 무장 직접 시험 피해는 선체 HP에 전달하지 않는다. 1/2층 시설 파괴의 다중 면 중첩은 임시 연결 면 규칙이다. 3층은 확정된 3F 상면/2F 측면/1F 하면 규칙을 적용했다. 수치와 코드 위치는 [가이드](PHASE2A_GUIDE.md)에 있다.

정식 사격·포탑 추적·레이더 공유는 2B, PD/함재기/포드는 2C, 선내 작전·생명유지 상실 인원 사망 타이머·나포는 이후다. 충각 피해, 정밀 함체 충돌, 자동 편대 정렬, FTL 여행, 키/핀 정책 R13, 장기 저장은 완료 범위에 포함하지 않는다.

검증 장면은 아군 2척+시험 표적 1척, 1/2/3배속이다. CPU Ryzen 9 3900X, GPU RTX 5070, 메모리 약 64 GB. 이번 검사는 성능 벤치마크가 아니며 대규모 함대 FPS/CPU/GPU 성능을 보장하지 않는다.
