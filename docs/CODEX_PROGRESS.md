# Codex 진행 기록

## 2026-11-12 조작 보완

`6d67970`의 깨끗한 `codex/phase1-playground`에서 시작했다. 디스크에서 적용할 AGENTS.md를 찾지 못해 대화에 제공된 개발 지침을 적용했다. `origin/work`의 `bca769f`를 fetch하고 검토 문서와 참조 자료를 `823dca3`에 반영했다.

`OVController`, `OVHUD`, `DefaultInput.ini`에 PlayGame3 기본 프로필과 Legacy 비교, 우클릭 XY 고정 및 단일 해제 확정, 뷰포트별 회전 제스처 소유권을 연결했다. 기존 V/Enter를 유지한다. 프로필 수치와 재현 절차는 가이드에 기록했다.

Win64 Development Editor 성공. 자동 17/17, PIE API 57/57 통과. Windows 화면 캡처 실패로 직접 입력은 확인 대기다. `P`로 프로필을 전환할 수 있는 ControlRange PIE를 일시정지 상태로 열어두었다. 다음 재개 시 직접 조작 4단계를 먼저 확인한다. [검증 결과](PHASE2A_VALIDATION.md#2026-11-12-조작-보완-검증)


갱신: 2026-11-12 (KST). 사용자 제공 CODEX DEVELOPMENT RULES v1.0을 적용한다.

## 목표와 완료 상태

최신 기획 `cb96d2c`의 2A-0 → 2A-1 → 2A-2를 기존 시험장 위에 구현하고 검증한다. **구현·빌드·자동 테스트·PIE API 검증 완료 / 실제 입력 검증 대기**다. 세부 기능·파일·남은 규칙은 [WORK_STATUS.md](WORK_STATUS.md), 실행법은 [PHASE2A_GUIDE.md](PHASE2A_GUIDE.md), 결과와 명령은 [PHASE2A_VALIDATION.md](PHASE2A_VALIDATION.md)에 있다.

- 저장소 `C:\Users\user\source\repos\A1`, 브랜치 `codex/phase1-playground`.
- 보존 이력: `c2f6627` 최초 구현, `4a32446` 부분 헤더, `274a55e` 최신 기획 병합, `873308c` 첫 2A 통합, `bc95a68` 보완된 소스·자산·검사 도구, **`379414d` 초광폭 카메라/포커스 보완**.
- 최종 빌드: UE 5.8.4 Win64 Development Editor 성공 6.27초. MSVC 14.51.36260 / Windows SDK 10.0.26100.0.
- 자동 테스트: 기존 이동 3개를 포함해 **15/15 통과**, 종료 0. 쉴드 부분 용량·층별 장갑·잘못된 데이터 거부 회귀 3개 추가.
- 실제 PIE API: **45/45 통과** (2026-11-12 07:45 KST), 오류 없음. 시간·이동·피해·선택·UI 입력 경계를 실제 PIE 월드에서 검사. OS 입력을 주입한 검사가 아니다.
- 1/2/3층 DataAsset을 UE AssetTools로 저장·검증. 기존 Content 맵과 이전 검증 자료 보존.

## 수정 파일과 기술적 결정

`OVShipLayout.*`, `OVShipSystemsComponent.*`, `OVShip.*`, `OVController.*`, `OVHUD.*`, `OVOrderInput.*`, `OVSystemsTests.cpp`, Config 2개, Content/Layouts 3개, Tools의 자산 생성/PIE 검사 스크립트를 추가·수정했다. 정의 자산과 개체 상태 분리, 단일 시뮬레이션 시간, 별도 장갑/슬롯/무장 ID, 표시 전용 프리뷰를 유지한다. 임시 수치는 DefaultGame.ini의 Tuning에 분리한다.

검사로 발견한 읽기 전용 Velocity 변경 오류는 테스트/DEV API 사용으로 고쳤다. 게임 결함인 Canvas 수명에 따른 입력 누출, 대표 함선 교체 시 배열 범위, 프리뷰 클리핑·리사이즈 상한, 부분 쉴드 재가동, 층별 장갑 파괴를 수정하고 해당 API/회귀 검사로 재확인했다. Blueprint/C++ 직렬화 이름은 유지하고 Python 이름 충돌만 ScriptName으로 분리했다.

## 미해결과 다음 순서

1. Windows computer-use 화면 캡처가 시간 초과하여 **직접 마우스·키보드 검증은 미실행**이다. 가이드의 8단계 체크리스트를 실행하고 결과를 별도로 추가한다. 렌더 화면은 엔진 Shot SHOWUI로 확인했다.
2. 한국어 엔진 자체 검사 오류 15건은 별도 기록하며 프로젝트 테스트 성공과 혼동하지 않는다.
3. R11/R13/R14와 1/2층 장갑 연동 임시 규칙은 정식 확정처럼 바꾸지 않는다. 실제 사격/탐지/포탑은 2B부터다.
4. 새 작업 전 Git 상태/소스 커밋/검증 증거를 비교한다. 기존 게임과 UE584BuildSmokeTest는 건드리지 않는다. 생성물은 커밋하지 않고 원격 push는 사용자 요청이 없으므로 하지 않는다.
