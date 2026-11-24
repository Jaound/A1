# 기지 건설 시험장 검증

현재 조작과 확장 모듈 결과는 [기지 2단계 기록](../BaseTrial2/README.md)에 있다. 아래는 1단계 실행 당시의 기록이다.

## 제출 기준

기획 `40bba6e0861a2027334b1254db19ba21720472aa`, 개발 시작 `c24ab8a`, 초기 건설 코드 `175ecb4`, 통합 `7fe5037`, 구현 `9a4802e`. Win64 Development Editor 빌드 성공. 실제 사용한 MSVC는 14.51.36260, Windows SDK는 10.0.26100.0이다. 빌드 도구에서 컴파일러가 선호 버전보다 새롭다는 경고가 있었다.

최종 빌드의 프로젝트 자동 검사 167개가 모두 통과했다. 엔진 시작 시 출력되는 한국어 자체 검사 오류는 별도이며, 프로젝트 검사 종료 코드는 0이었다. [자동 검사별 결과](AutomationSummary.txt)

PIE 검사 34개가 통과했다. 정지 및 세 배속에서의 시간, 선택 함선의 기존 목적지, 다른 임무와의 격리, 임무 재생성·정리, 세 수평 접합과 수직 허브 접합을 확인했다. 검증한 세 수평 접합의 소켓 중심 간격은 각각 0cm, 바깥 방향 내적은 −1, 상하 방향 내적은 +1이었다. 이 값은 측정해 등록한 소켓 기준이다. 수직 허브도 중심 5cm 이내와 양 방향 조건을 충족했다. 임무 카드는 실제 UE 장면에서 잘라낸 이미지다.

## 직접 입력

Windows 별도 게임 창(DX11, 1280×740, UI 80%)에서 타이틀 → 임무 선택 → 기지 건설 시험장 → 작전 출격을 실제 마우스로 진행했다.

| 확인한 동작 | 화면에서 확인한 결과 |
| --- | --- |
| 코어 바깥에 일자 통로 예약, 이어서 창고 예약 | 일반 자원 1,200 → 1,160 → 860. 통로 15.0초, 창고 연결 대기 60.0초 |
| 시설 선택으로 통로를 다시 선택하고 공사 취소 | 창고가 사용 중이라 취소 차단. 자원 860 유지 |
| 시간 재개, 함대 화면 이동, 3배속으로 진행 후 기지 복귀·일시정지 | 아군 초계함이 선택된 채로 복귀. 창고가 연결 대기에서 공사 중 16.4초로 바뀜 |
| 분기 허브 예약과 전액 환불 | 자원 860 → 820 → 860 |
| PageUp, E | 배치 평면 25m, 회전 90° |
| 재시작, 목록 복귀 | 1,200, 공사 0건, 0m·0°, 일시정지·1배속. 임무 목록에 기지 카드가 선택됨 |

위 흐름은 원본 메시 반입 후 첫 실행본에서 확인했다. 이후 연결 고리의 소켓 검사, 모델 경계, 카메라·격자 표시와 카드 이미지를 보완했다. 최종 코드의 검사는 아래 PIE/API 및 자동 검사로 재실행했다.

## 재현

- 일반 실행은 [조작 안내](../../BASE_BUILD_CHECKPOINT.md)를 따른다. 별도 게임을 직접 열려면 `UnrealEditor.exe OrbitalVanguard.uproject /Game/Maps/ControlRange -game -windowed -ResX=1280 -ResY=740 -d3d11`.
- Win64 에디터는 UE 5.8.4의 `Build.bat OrbitalVanguardEditor Win64 Development <uproject> -WaitMutex -NoHotReloadFromIDE -NoUBTMakefiles`로 빌드한다.
- 자동 검사는 `UnrealEditor-Cmd.exe <uproject> /Game/Maps/ControlRange -unattended -nullrhi -nosound -ExecCmds="Automation RunTests OrbitalVanguard" -TestExit="Automation Test Queue Empty"`. 신규 ID는 `OrbitalVanguard.Base.TrialNetwork`다.
- PIE는 `UnrealEditor-Cmd.exe <uproject> /Game/Maps/ControlRange -d3d11 -customconfig=HeroArtDX11 -unattended -nosound -ExecCmds="py <프로젝트>/Tools/ValidateBaseTrial.py"`. 개별 결과와 렌더는 프로젝트의 `Saved/Validation/BaseTrial`에 생성된다. [개별 검사](PIE.json) · [실행 화면](BaseTrial.png)
- 네 FBX 반입은 오류 0, 고유 경고 10건이었다. 스무딩 그룹과 일부 길이가 거의 0인 탄젠트·바이노멀에 대한 경고가 있다. 원본 메시의 모든 폴리곤 품질이나 모든 회전 조합을 시각 승인한 것은 아니다. [반입 수치와 소켓 좌표](Ports.json)
- C++ 룰 검사 및 PIE에는 Windows 입력 자동화를 사용하지 않는다. 마우스 검증은 위 표에서 따로 기록했다. 패키징은 이번 검증 범위에 포함하지 않았다.
