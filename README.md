# 오비탈 뱅가드(가제)

싱글플레이 지속형 우주 세력 RTS. 3D 함대전과 2D 선내 작전, 기지·함대·영웅 성장을 연결합니다.

## 기획을 읽을 때

**[문서 지도·용어·관리 원칙](docs/DESIGN_INDEX.md)**에서 시작합니다. 전체 요약은 [GDD](docs/GDD.md), 변경 근거는 [결정 기록](docs/DECISIONS.md), 아직 선택할 정책은 [미결정 목록](docs/OPEN_QUESTIONS.md)에서 확인합니다.

| 분야별 현행 명세 | 담당 범위 |
| --- | --- |
| [입체 모듈 기지](docs/BASE_SYSTEMS.md) | 코어·연결·시설·접안·조선·방어의 확정 구조 |
| [전략·경제](docs/STRATEGY_ECONOMY.md) | 성계·자원·물류·전초 전문화·AI 시간 |
| [함대 전투](docs/FLEET_COMBAT.md) | 함급·명령·기동·무장·탐지·PD·FTL |
| [함선 시스템](docs/SHIP_SYSTEMS.md) | 시설·인원·레이아웃·장갑·쉴드·피해·수리·생존 |
| [모함·격납고](docs/CARRIER_OPERATIONS.md) | 함재기/수납 소형함·용량·소유·출격/귀환·관제 |
| [영웅](docs/HERO_SYSTEMS.md) | 임무·유형·보직·성장 |
| [강습·선내 작전](docs/BOARDING_COMBAT.md) | 포드/강습선·진입·교전·점령·나포·탈출 |
| [UI·입력](docs/UI_SYSTEMS.md) | 화면·카메라·창·조작·정보 표시 |

## 제작을 이어갈 때

[현재 구현 상태](docs/IMPLEMENTATION_STATUS.md) → [제작 순서](docs/IMPLEMENTATION_PLAN.md) → [2단계/2B 계획](docs/PHASE2_PLAN.md) → [UE5 공통 기준](docs/UE5_IMPLEMENTATION_CONTRACT.md)을 읽습니다.

기획은 `work`, 게임 코드는 `codex/phase1-playground`에서 관리합니다. 2026-11-16 확인한 코드 최신은 `be544d7`(2A 조작 보완)입니다. 사용자 조작 확인 후 2B 진행이 요청됐으며 실제 전투 완료 기록은 아직 없습니다. 새 커밋이 생기면 소스와 증거를 확인해 상태를 갱신합니다.

## 검토·참조·과거 기록

- [통합 검토/충돌 수정 이력](docs/PLANNING_REVIEW.md)
- [프로토타입 개선 기록](docs/PROTOTYPE_IMPROVEMENT_PLAN.md), [시행착오·회귀 체크](docs/DEVELOPMENT_PITFALLS.md)
- [2A 당시 검토](docs/PHASE2A_REVIEW.md), [PlayGame3 당시 분석·이식안](docs/PLAY_GAME_3_CONTROLS.md), [카메라 참조 코드/검사](reference/PlayGame3Controls/README.md)
- 과거 [결정1~79 원문](docs/archive/DECISIONS_2026-11-11.md), [개선안 원문](docs/archive/PROTOTYPE_IMPROVEMENT_PLAN_2026-11-11.md), [시행착오 원문](docs/archive/DEVELOPMENT_PITFALLS_2026-11-11.md)

확정 기획·미결정·시험 기준값·임시 구현·검증 결과를 구분합니다. 과거 기록과 프로토타입 코드는 현행 분야 명세를 덮어쓰지 않습니다.

새로 제안한 [기지·생산·연구 밸런스v0.1](docs/BASE_BALANCE_DRAFT.md)은 비용·시간·수입·연구 경로의 **미실측 초안**입니다. 확정 구조와 새 제안 수치를 분리해 읽습니다.
