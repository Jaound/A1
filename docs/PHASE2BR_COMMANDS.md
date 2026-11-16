# 2B-R R1 목적 명령·보호 사용법

> 최신 R2·R3 실행은 [D116 교정 및 종합 검증](PHASE2BR_D116_CALIBRATION.md)을 확인한다. 아래는 이전 단계의 실행/검증 기록이다.

기획035a4fba81d26f1d67daa1f44bded0f7e8d3a434, 구현8228f04. R2 교정과 R3 전체 비교는 아직 완료하지 않았다. 기존 카메라/XYZ/독립 자동화와 쉴드·인원 구현을 유지한다.

Tools/RunEditor.ps1 → Play → F5 → 아군 선택 → T 후 적 선택. 왼쪽 전투 패널의 HULL/GUNS는 공격 목적만 선택한다. 수동에서는 B 또는 L로 발사 요청하고 F1로 시뮬레이션을 재개한다. GUNS는 실제 관측 장착점을 기존 조준/탄도로 공격한다. 빗나간 선체 피해와 다른 무장 명중이 가능하며 명중 보장이나 선체 면역이 없다. 살아 있는 관측 무장이 없으면 대기한다.

HOLD 버튼은 UNTIL EXIT(기본) → CONTINUE → UNTIL GOAL 순으로 전환한다. DEV 모드에서 DEV INSIDE는 선택 표적 내부의 **현재 사격 세력** 잔류 입력을 켜고 끈다. DEV GOAL은 목표 완료를 전환한다. 노란 DEV 문구는 실제 영웅이 아닌 주입 상태임을 표시한다. 다른 세력의 영웅 로스터를 공개하지 않는다.

보호된 표적에 FORCE GUN!을 누르면 현재 선택한 공격 무장만의 명시적 강제 요청을 보낸다. 선체 피격 위험 문구를 확인한다. 다른 무장·표적·함재기에 보호 예외를 공유하지 않는다. CANCEL FIRE/표적 변경/자동화 변경/이동·정지 등 다른 명령/나포는 예외를 해제한다. 이미 발사된 탄은 계속 비행한다. 일반 자동 교전은 보호된 표적을 제외하며 모두 보호되면 대기한다.

후속 구현은 Systems.SetInteriorPresence(세력,잔류,목표완료,DEV여부)를 통해 상태를 공급한다. 기본은 잔류 없음이다. 자체 세력 생존자 입력은 승무원0만으로 중립화되는 것을 막지만 시설 인원을 만들거나 함포를 가동하지 않는다. 개별 영웅/포드/보딩 AI는 구현하지 않았다.

## 검증

- 자동35/35: 기존32 유지, 새 Revision.DisableObservedWeapons/FactionProtectionAndForceScope/ProtectionSalvoAndSurvivorBoundary.
- PIE60/60: ValidateRevisionOrders.py는 기존 ValidatePhase2B.py의50개를 그대로 호출하며 앞에10개 신규 명령/보호 API 검사를 추가한다. 원본50개 스크립트는 보존했다.
- 2A PIE57/57. 최종 Win64 Development Editor 빌드6.81초.
- 로그: Saved/Logs/RevisionR1OrdersFinalBuild.log, RevisionR1OrdersAutomation.log, RevisionR1OrdersPIE.log, RevisionR1Orders2APIE.log. 제출 증거: validation/Phase2BR-Orders-*.
- 직접 OS 입력은 이번에 수행하지 않았다. API 결과로 실제 클릭/조작감을 주장하지 않는다. 한국어 엔진 자체 검사15건과 기존 컴파일러 경고는 별도로 남겼다.

자동 재실행은 기존 PHASE2B_GUIDE 명령을 사용한다. 새 PIE 스크립트 경로는 Tools/ValidateRevisionOrders.py, 보고서는 Saved/Validation/Phase2BRCommands.json이다. 자동 검사 필터 OrbitalVanguard는35개를 포함한다.

## 짧은 직접 확인

1. F5 → T로 적 선택 → GUNS → B → F1. 표적 선택만으로 발사하지 않고 포탑 조준 뒤 발사되는지 확인.
2. 일시정지 후 DEV INSIDE → 재개. 새 발사/잔여 살보는 막히고 이미 발사된 탄은 계속되는지 확인.
3. UNTIL GOAL에서 DEV GOAL, UNTIL EXIT에서 DEV INSIDE 해제로 각각 보류가 풀리는지 확인.
4. FORCE GUN!으로 선택 공격 무장만 발사하고 X 정지/표적 변경/CANCEL FIRE 후 강제 예외가 해제되는지 확인. 카메라·상세 창·XYZ 입력도 함께 확인.
