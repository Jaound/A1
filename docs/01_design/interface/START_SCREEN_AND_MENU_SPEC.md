# 게임 시작 화면 및 메뉴 구성 기획서 (Orbital Vanguard · UE 5.8)

> **기준 원격 상태**: 기획 브랜치 `work` (`f0ac7fb`) · UE 5.8 코드 브랜치 `codex/phase1-playground` (`62adc17`)
> **작성 배경**: 현재 `OrbitalVanguard.uproject`(UE 5.8, DX11)는 `ControlRange` 맵 직진입 후 상단 Canvas `장면` 메뉴(`bSceneMenuOpen`) 및 `F5/F6/F7` 단축키로 6종 플레이 시나리오를 호출하는 구조입니다. 본 문서는 기존 C++ 서브시스템(`UOVPlayScenarioSubsystem`, `AOVController`, `AOVHUD`)과 자동/PIE 회귀 경로를 유지하면서 정식 **시작 화면 · 임무 선택 · 일시정지 · 환경 설정 · 작전 결과 메뉴**를 UMG/Slate로 구축하기 위한 기획 명세입니다.

---

## 1. 현재 UE 5.8 진행 현황 및 진입 구조 분석

| 계층 | 현재 구현 상태 (`62adc17`) | 메뉴 연동 포인트 |
| --- | --- | --- |
| 부트 및 맵 | `GameDefaultMap=/Game/Maps/ControlRange`, `OVGameMode` | 별도 타이틀 맵 분리 없이 `ControlRange` 궤도 카메라 위 UMG 오버레이 마운트 |
| 플레이 시나리오 | `UOVPlayScenarioSubsystem` 6종 정의 (`Fleet100/200/300`, `Sandbox` 가동 / `Logistics`, `Evacuation` 준비 중) | `Definitions()`, `Start(Controller, Id)`, `Clear()` API 직접 바인딩 |
| 조작 및 HUD | `PlayGame3` 기본 프로필 + `Legacy` 비교(`P`), Canvas 전장 HUD + UMG 카드/영웅 위젯 | 환경 설정 조작 탭, `UIScale` DPI 보정, D122 창 화면 안 복구 연결 |
| 자동화·PIE 회귀 | C++ 자동 134/134, PIE 51/51, 2C API 83/83 (`-OVPlayScene=` CLI 인자 사용) | CLI 인자 또는 자동화 테스트 감지 시 타이틀 화면 자동 우회 필수 |
| 미결정 연계 | `R13`(ESC·키 배치), `R27`(자동 일시정지 옵션), 카드·결과 UI 후속 예정 | 본 메뉴 기획에서 ESC 4단계 우선순위 및 R27 설정 매트릭스 확정 제안 |

---

## 2. 전체 메뉴 상태 전이 흐름 (State Machine)

1. **부트 판별 (`AOVController::BeginPlay`)**
   - `-OVPlayScene=<Id>` 인자 또는 자동화/API 직접 호출 존재 → **전장 즉시 진입** (기존 51/51 PIE 회귀 무손상).
   - 일반 실행 → **01. 타이틀 화면 (`WBP_OVTitleScreen`)** 진입 (시간 정지, 궤도 카메라 완속 패닝).
2. **타이틀 → 임무 선택 (`WBP_OVScenarioSelect`)**
   - 6종 플레이 시나리오 카드 표시. 가동 항목(`bAvailable == true`) 선택 시 `UOVPlayScenarioSubsystem::Start(Controller, Id)` 호출 후 전투 HUD로 전환.
3. **전투 중 ↔ 일시정지 메뉴 (`WBP_OVPauseMenu`)**
   - ESC 우선순위(섹션 5) 또는 좌상단 `메뉴` 버튼으로 진입. 시간 배속 0 고정, 전장 입력 투과 차단.
4. **환경 설정 (`WBP_OVSettingsMenu`)**
   - 타이틀 및 일시정지 양쪽에서 공용 모달 스택으로 호출. `GameUserSettings.ini` 영구 저장.
5. **전투 종료 → 작전 결과 화면 (`WBP_OVResultScreen`)**
   - `EOVPlaySceneResult != Running` 전이 시 자동 마운트. `재시작`, `임무 목록 복귀`, `전장 둘러보기` 제공.

---

## 3. 화면별 상세 기획 명세

### 3.1 타이틀 / 시작 화면 (`WBP_OVTitleScreen`)
- **레이아웃 원칙**: 좌측 35% 정렬 메뉴 컬럼(다크 스크림 확보) + 우측 65% 궤도 함대 배경 뷰포트. 불필요한 뉴스 배너·텔레메트리 티커·장식 점멸 도트 금지.
- **메뉴 항목 (위에서 아래 순서)**:
  1. **작전 개시 · 임무 선택** *(Primary CTA)* — `WBP_OVScenarioSelect`로 이동.
  2. **캠페인** *(4단계 예정 · 비활성 안내)* — 기지/연구/경제/저장(4단계) 구현 전까지 잠금 상태 및 예정 로드맵 툴팁 표시.
  3. **자유 전투 시험장** — `Start(Controller, "Sandbox")` 즉시 호출(기존 Insert 개발 도구 연계).
  4. **환경 설정** — `WBP_OVSettingsMenu` 열기.
  5. **종료** — 확인 모달 없이 데스크톱 종료(`UKismetSystemLibrary::QuitGame`; PIE에서는 안내 문구 표시).
- **하단 정적 메타데이터 (비박스 인라인 텍스트)**: 현재 조작 프로필(`PlayGame3`), 단축키 안내(`F5~F7 빠른 함대전`).

### 3.2 플레이 임무 선택 화면 (`WBP_OVScenarioSelect`)
- **2단 분할 구조**: 좌측 6종 임무 카드 목록(42%) + 우측 선택 임무 브리핑(58%).
- **6종 임무 바인딩 명세 (`OVPlayScenarioSubsystem.cpp` 일치)**:

| ID | 임무명 | 상태 | 한쪽 코스트 | 격납고 / 함재기 | 제한 규칙 | 단축키 |
| --- | --- | --- | ---: | ---: | --- | --- |
| `Fleet100` | 100코스트 함대전 | 가동 | 100 | 2 / 20 | 교착 120s · 제한 900s | `F5` |
| `Fleet200` | 200코스트 함대전 | 가동 | 200 | 2 / 20 | 교착 120s · 제한 900s | `F6` |
| `Fleet300` | 300코스트 함대전 | 가동 | 300 | 4 / 40 | 교착 120s · 제한 900s | `F7` |
| `Logistics` | 채굴·수송 엄호 | 준비 중 | 미정 | - | 중형 수송선·물류 연동 후속 | - |
| `Evacuation` | 공습 생존·퇴각 | 준비 중 | 미정 | - | 예고 파동·FTL 회수/포기 후속 | - |
| `Sandbox` | 자유 전투 시험장 | 가동 | 자유 | 자유 | 개발 도구 배치 전용 | - |

- **우측 브리핑 필수 요소**: 임무 목표, 함체별 편성 수량·코스트 표(초계함~구축전함/항공순양함), 승패·교착·이탈 판정 요약, 영웅/캠페인 명단 격리 고지, 하단 단일 Primary CTA **`작전 출격`**.
- `bAvailable == false` 항목 선택 시에도 기획 개요와 다음 개발 마일스톤을 확인할 수 있으나 `작전 출격` 버튼은 비활성화(`준비 중`)한다.

### 3.3 일시정지 / 시스템 메뉴 (`WBP_OVPauseMenu`)
- **항목**: `전투 재개`, `임무 재시작` (확인 후 `Start(ActiveId)`), `임무 목록으로 복귀` (`Clear()` 후 선택 화면), `조작 · 환경 설정`, `타이틀 화면으로`.
- **세션 보호 규칙**: 전투 일시정지 진입 시 현재 시간 배속(`F1~F4`)을 기억하고 0배속으로 동결하며, 재개 시 직전 배속으로 복원한다.

### 3.4 환경 설정 메뉴 (`WBP_OVSettingsMenu`)
1. **조작 탭 (`OVControlProfile`)**
   - 조작 프로필 선택: `PlayGame3 (기본)` / `Legacy` (기존 `P` 토글과 양방향 동기화).
   - 카메라 회전 감도, 줌 속도, 고도 드래그 임계(`8px` 기본), 적함 우클릭 표적 임계(`25px` 기본) 확인 및 조정.
   - 핵심 단축키 일람표(`F1~F4`, `Space`, `1~9`, `Ctrl+1~9`, `V/Enter`, `R`, `F`, `M`, `B`).
2. **전술 · 자동 일시정지 탭 (`R27`)**
   - D122/D123 긴급 사건별 자동 일시정지 개별 체크박스:
     - [x] 아군 함선 선내 침입 발생 시 자동 정지
     - [x] 아군 주력함 쉴드 비활성 전환 시 자동 정지
     - [x] 함재기 생존 10% 이하 / 전멸 경보 시 자동 정지
     - [x] 태양 플레어 도달 경고 / 블랙홀 견인 위험 시 자동 정지
     - [ ] 일반 아군 피격 시 자동 정지 *(기본 꺼짐)*
3. **화면 · UI 탭**
   - 해상도, 창 모드(전체 창 / 창 모드), 프레임 제한, DX11 렌더링 옵션 안내.
   - `UIScale` 배율 슬라이더(`0.80 ~ 1.50`) 및 D122 **`플로팅 창 화면 안 복구`** 즉시 실행 버튼.
4. **오디오 탭**
   - 마스터, 전투 효과음, D122 경보·무전 알림, 우주 배경음 개별 볼륨 및 음소거.

### 3.5 작전 결과 화면 (`WBP_OVResultScreen`)
- `EOVPlaySceneResult` 6개 종료 상태를 모두 명시적으로 구분 표시한다:
  - `Victory` (승리) · `Defeat` (패배) · `MutualDestruction` (쌍방 전멸) · `NoCombatCapability` (전투 불능 무승부) · `Stalemate` (120초 교착) · `TimeLimit` (900초 시간 초과).
- **핵심 지표 (고정폭 숫자 정렬)**: 경과 게임 시간(`ElapsedSeconds`), 아군/적군 잔존 코스트 대 초기 코스트(`InitialCosts`), 함재기 생존/초기(`InitialAircraft`), **이탈 함선 수(완파와 별도 집계)**.
- **행동 버튼**: `같은 임무 재시작`, `임무 목록으로 복귀`, `전장 둘러보기`(결과창 임시 최소화 토글).

### 3.6 시작 화면 배경 함선 교체 전략 (인게임 모델링 · 사전 렌더 파이프라인)
- **교체 원칙**: 기획 와이어프레임의 배경 이미지는 임시 레퍼런스이며, 실제 UE 5.8 적용 시에는 `Unreal/OrbitalVanguard/Content/Presentation/Ships/`에 이미 임포트된 **인게임 함선 모델링(`SM_Hull`)** 또는 **동일 모델을 UE 에디터에서 사전 촬영한 2D 렌더 이미지**로 교체한다.
- **이중 지원 DataAsset (`UOVTitleBackdropDataAsset`)**:
  - **방식 A · 실시간 3D 표시 프록시 (`LiveProxyStage`, 기본 권장)**:
    - 기존 `OVShipPreview`와 동일하게 실제 전투 액터(`AOVShip`)를 생성하지 않는 표시 전용 `AOVTitleBackdropStage` 액터에 `UStaticMeshComponent`만 조립하고 `/Game/Presentation/Environment/SpaceSkybox/SM_Source` 위에서 완속 궤도 카메라 패닝을 수행한다.
    - `WBP_OVScenarioSelect`에서 임무를 선택하면 배경 편대도 해당 임무 주력함(`BattleshipFull` / `DreadnoughtFull` / `BattlecarrierFull` / `TransportFull`)으로 즉시 교체한다.
    - 전투 시작(`UOVPlayScenarioSubsystem::Start`) 즉시 프록시 스테이지를 비활성화/제거해 전투 성능 영향을 0으로 유지한다.
  - **방식 B · 인게임 모델 사전 렌더 이미지 (`PreRenderedInGameImage`, 경량 폴백)**:
    - 저사양·초기 로딩 최적화·`-nullrhi` 환경에서는 이미 활성화된 `PythonScriptPlugin` / `EditorScriptingUtilities` 스크립트로 동일 `SM_Hull` 편대를 16:9 구도로 캡처한 `/Game/UI/Title/T_TitleBackdrop_*.uasset`(`UTexture2D`)을 UMG `Image` 위젯에 바인딩한다.
    - 함선 모델이나 재질이 갱신될 때 캡처 스크립트만 재실행하면 타이틀 이미지도 항상 인게임 실물과 일치한다.
- **기본 타이틀 편대 실물 에셋 매핑**:
  - 중앙 기함: `/Game/Presentation/Ships/DreadnoughtFull/SM_Hull` (구축전함)
  - 후방 모함: `/Game/Presentation/Ships/CarrierFull/SM_Hull` (항공모함)
  - 호위 편대: `/Game/Presentation/Ships/BastionFull/SM_Hull`, `/Game/Presentation/Ships/VectorFull/SM_Hull`
  - 초계 편대기: `/Game/Presentation/Ships/FighterFull/SM_Hull` ×4
  - 항공 타격단 / 물류 호송단 교체 시: `BattlecarrierFull/SM_Hull`, `BattleshipFull/SM_Hull`, `Air_DefenseFull/SM_Hull`, `SM_PC14_FullMesh`, `TransportFull/SM_Hull`, `MiningFull/SM_Hull`

---

## 4. R13 ESC 입력 우선순위 및 PIE 공존 규칙

| 우선순위 | 현재 상태 | ESC 동작 | 비고 |
| --- | --- | --- | --- |
| 1 | 환경 설정 / 확인 모달 / 열린 팝업 존재 | 최상위 모달 닫기 | 전장 상태 불변 |
| 2 | 일시정지 메뉴 열림 | 일시정지 메뉴 닫고 전투 재개 | 직전 배속 복원 |
| 3 | 이동 프리뷰 / 표적 지정 진행 중 | 현재 입력 제스처 취소 | 선택 함선 유지 |
| 4 | 함선 선택 상태 | 함선 선택 해제 | 상세창 닫기 연계 |
| 5 | 전장 기본 상태 (미선택) | 일시정지 메뉴 열기 | PIE 종료는 `Shift+ESC` 유지 |

---

## 5. UE 5.8 구현 체크리스트 (로컬 에디터 작업용)

- [ ] `UOVTitleBackdropDataAsset` 및 `AOVTitleBackdropStage` 구현 (`/Game/Presentation/Ships/*Full/SM_Hull` 표시 전용 프록시 + `T_TitleBackdrop_*` 2D 폴백 전환)
- [ ] UE 에디터 Python 캡처 스크립트 추가(`Tools/CaptureTitleBackdrops.py`)로 인게임 모델 변경 시 2D 타이틀 이미지 자동 갱신
- [ ] `UOVGameUserSettings` 클래스 추가 및 `DefaultEngine.ini` 등록 (`PlayGame3/Legacy`, `UIScale`, `R27` 토글 저장)
- [ ] `AOVController`에 UI 상태 머신(`Title / ScenarioSelect / Playing / Paused / Result`) 및 `-OVPlayScene=` 우회 분기 추가
- [ ] `WBP_OVTitleScreen`, `WBP_OVScenarioSelect`, `WBP_OVPauseMenu`, `WBP_OVSettingsMenu`, `WBP_OVResultScreen` UMG 위젯 구현
- [ ] `AOVHUD`의 기존 `bSceneMenuOpen` Canvas 메뉴는 개발 모드(`bDeveloperMode`) 폴백으로 축소하고 일반 플레이는 UMG 메뉴로 통합
- [ ] 기존 회귀 검증 유지: C++ 자동 134/134 + `ValidatePlayScenarioFleets.py` PIE 51/51 + `Legacy2C_PIE` 83/83 통과 확인
