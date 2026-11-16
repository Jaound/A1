# 오비탈 뱅가드(가제)

싱글플레이 지속형 우주 세력 RTS. 3D 함대전과 2D 선내 작전, 기지·함대·영웅 성장을 연결합니다.

## 기획을 읽을 때

**[문서 지도·용어·관리 원칙](docs/DESIGN_INDEX.md)**에서 시작합니다. 전체 요약은 [GDD](docs/00_overview/GDD.md), 변경 근거는 [결정 기록](docs/00_overview/DECISIONS.md), 아직 선택할 정책은 [미결정 목록](docs/00_overview/OPEN_QUESTIONS.md)에서 확인합니다.

| 분야별 현행 명세 | 담당 범위 |
| --- | --- |
| [입체 모듈 기지](docs/01_design/strategy/BASE_SYSTEMS.md) | 코어·연결·시설·접안·조선·방어의 확정 구조 |
| [전략·경제](docs/01_design/strategy/STRATEGY_ECONOMY.md) | 성계·자원·물류·전초 전문화·AI 시간 |
| [함대 전투](docs/01_design/fleet/FLEET_COMBAT.md) | 함급·명령·기동·무장·탐지·PD·FTL |
| [함선 시스템](docs/01_design/fleet/SHIP_SYSTEMS.md) | 시설·인원·레이아웃·장갑·쉴드·피해·수리·생존 |
| [모함·격납고](docs/01_design/fleet/CARRIER_OPERATIONS.md) | 함재기/수납 소형함·용량·소유·출격/귀환·관제 |
| [영웅](docs/01_design/boarding/HERO_SYSTEMS.md) | 임무·유형·보직·성장 |
| [강습·선내 작전](docs/01_design/boarding/BOARDING_COMBAT.md) | 포드/강습선·진입·교전·점령·나포·탈출 |
| [UI·입력](docs/01_design/interface/UI_SYSTEMS.md) | 화면·카메라·창·조작·정보 표시 |

## 제작을 이어갈 때

[현재 구현 상태](docs/03_development/IMPLEMENTATION_STATUS.md) → [제작 순서](docs/03_development/IMPLEMENTATION_PLAN.md) → [2단계/2B 계획](docs/03_development/PHASE2_PLAN.md) → [UE5 공통 기준](docs/03_development/UE5_IMPLEMENTATION_CONTRACT.md)을 읽습니다.

기획은 `work`, 게임 코드는 `codex/phase1-playground`에서 관리합니다. 2026-11-16 확인한 코드 최신은 `be544d7`(2A 조작 보완)입니다. 사용자 조작 확인 후 2B 진행이 요청됐으며 실제 전투 완료 기록은 아직 없습니다. 새 커밋이 생기면 소스와 증거를 확인해 상태를 갱신합니다.

## 검토·참조·과거 기록

- [통합 검토/충돌 수정 이력](docs/04_analysis/PLANNING_REVIEW.md)
- [프로토타입 개선 기록](docs/05_references/PROTOTYPE_IMPROVEMENT_PLAN.md), [시행착오·회귀 체크](docs/03_development/DEVELOPMENT_PITFALLS.md)
- [2A 당시 검토](docs/archive/analysis/PHASE2A_REVIEW.md), [PlayGame3 당시 분석·이식안](docs/05_references/PLAY_GAME_3_CONTROLS.md), [카메라 참조 코드/검사](reference/PlayGame3Controls/README.md)
- 과거 [결정1~79 원문](docs/archive/DECISIONS_2026-11-11.md), [개선안 원문](docs/archive/PROTOTYPE_IMPROVEMENT_PLAN_2026-11-11.md), [시행착오 원문](docs/archive/DEVELOPMENT_PITFALLS_2026-11-11.md)

확정 기획·미결정·시험 기준값·임시 구현·검증 결과를 구분합니다. 과거 기록과 프로토타입 코드는 현행 분야 명세를 덮어쓰지 않습니다.

새로 제안한 [기지·생산·연구 밸런스v0.1](docs/02_balance/BASE_BALANCE_DRAFT.md)은 비용·시간·수입·연구 경로의 **미실측 초안**입니다. 확정 구조와 새 제안 수치를 분리해 읽습니다.

[실제5시간 캠페인 경제·진행 분석](docs/archive/analysis/CAMPAIGN_SIMULATION.md): 비용/생산/연구·배속·가상6/10임무를 비교한360회 모형 실행입니다. 실제 전투·UE 플레이 검증과 구분합니다.

[후속 프로토타입 업데이트 기록](docs/05_references/PROTOTYPE_FBX_UPDATE_RECORD.md): FBX 선체·분리 포탑·함급 변형·전장 확대·뷰어·검증/배포 보고를 보관합니다. 모델 도입과 UE 수치 적용은 보류하며 사용자 보고와 독립 검증을 구분합니다.

현행 구축함은 **백터급(Vector DD)·바스티온급(Bastion DD)** 두 함체이며 구 단일 구축함 항목/가격은 현행 기획에서 대체했습니다. [함대 명세](docs/01_design/fleet/FLEET_COMBAT.md)와 [R31](docs/00_overview/OPEN_QUESTIONS.md#r31)을 확인합니다. 캠페인 모형은 구입력의 역사 분석입니다. 최신WebZIP 실물 분석은32MiB전송상한으로 보류되어 [접근 상태](docs/05_references/PROTOTYPE_FBX_UPDATE_RECORD.md)를 구분해 기록했습니다.

[전투 수치v0.2](docs/archive/balance/COMBAT_BALANCE.md) · [함대/영웅/강습 연동 모형 분석](docs/archive/analysis/COMBAT_SIMULATION.md) · [재현 도구](reference/CombatSimulation/README.md): 신규 전투 프로필의 반복 검토 자료다. UE 적용·최신WebZIP 실행·전체 캠페인 완성 검증이 아니다.

외부전 목표는 **1배속10~15분**이다. D112 당시 검토는 [장기전 수치v0.3](docs/02_balance/FLEET_ENDURANCE_BALANCE.md) · [전 함종/코스트100·200·300 분석](docs/04_analysis/FLEET_ENDURANCE_RESULTS.md) · [재현 도구](reference/FleetEndurance/README.md)에 보존한다. D116 현행 시험은 아래v0.6을 따른다. 이전v0.2는 비교/선내 영웅 출발값이며 새 함대 시간 기준을 덮어쓰지 않는다.

[D113 강습·영웅 수·코스트 시간 재검토](docs/04_analysis/BOARDING_REVIEW.md): 초대형 화력 유지, 개인별1포드, 강습선 공격0/PD1 확정과 이전 모형의PD/방어 영웅 가정을 정정한다.

[D114 영웅 상세v0.5](docs/01_design/boarding/HERO_SYSTEMS.md) · [수리·보급v0.5](docs/01_design/fleet/REPAIR_LOGISTICS.md): 전체세력 동시15명·성장형 전력, 현장 수리 자원1.5배/속도50%와 설계값. D114 작성 당시에는 시뮬레이션을 보류했고 이후 D116에서 통합 시험을 재개했다.

D115: 영웅을 **초능력자·강화인간**으로 정리하고 능력/장비 표시 명칭을 맞췄습니다. 기존 수치·전체 세력15명 한도·영구 사망·수리/점령 제약은 유지합니다. 누적 문서는 [용도/분야별 폴더](docs/README.md)로 정리했으며 [경로 대응표](docs/DOCUMENT_PATHS.json)를 제공합니다.

D116: [통합 전투 기준v0.6](docs/02_balance/INTEGRATED_COMBAT_BALANCE.md) · [반복 시험 결과](docs/04_analysis/INTEGRATED_COMBAT_RESULTS.md) · [재현 도구](reference/IntegratedCombat/README.md). 강습 대상 보호/무장 무력화, 무지연·인원 기반 쉴드, 적 정보 비공개/비대칭 임무, 함재기 연구·개조를 적용한 설계 모형이며 UE 구현과 구분합니다.
