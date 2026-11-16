# 기획 폴더 안내

처음에는 [문서 지도](DESIGN_INDEX.md)를 읽는다. 문서는 **용도와 상태 → 게임 분야** 순서로 나눈다. 폴더 번호는 탐색 순서이며 게임 구현 단계 번호가 아니다.

| 폴더 | 정리 기준 | 대표 문서 |
| --- | --- | --- |
| `00_overview/` | 게임 전체 개요·사용자 결정·미결정 정책 | [GDD](00_overview/GDD.md), [결정](00_overview/DECISIONS.md), [R목록](00_overview/OPEN_QUESTIONS.md) |
| `01_design/fleet/` | 외부 함대전·함체/시설·모함·수리 물류 | [함대](01_design/fleet/FLEET_COMBAT.md), [함선](01_design/fleet/SHIP_SYSTEMS.md), [수리](01_design/fleet/REPAIR_LOGISTICS.md) |
| `01_design/boarding/` | 영웅 정체성/성장/스킬과 선내 작전 | [영웅](01_design/boarding/HERO_SYSTEMS.md), [강습](01_design/boarding/BOARDING_COMBAT.md) |
| `01_design/strategy/` | 입체 기지·성계·자원·경영 | [기지](01_design/strategy/BASE_SYSTEMS.md), [전략/경제](01_design/strategy/STRATEGY_ECONOMY.md) |
| `01_design/interface/` | 화면·조작·정보 표시 | [UI](01_design/interface/UI_SYSTEMS.md) |
| `02_balance/` | 여러 시스템을 가로지르는 현행 수치 기준안·생산/연구 표 | [기지/생산](02_balance/BASE_BALANCE_DRAFT.md), [통합 전투](02_balance/INTEGRATED_COMBAT_BALANCE.md) |
| `03_development/` | 구현 순서·실제 완료 상태·UE 기준·회귀 체크 | [구현 상태](03_development/IMPLEMENTATION_STATUS.md), [제작 순서](03_development/IMPLEMENTATION_PLAN.md) |
| `04_analysis/` | 현재도 참고하는 분석 결과·한계·정리 근거 | [강습 공방 검토](04_analysis/BOARDING_REVIEW.md), [장기전 결과](04_analysis/FLEET_ENDURANCE_RESULTS.md) |
| `05_references/` | 다른 프로토타입의 관측·이식 자료 | [조작 이식](05_references/PLAY_GAME_3_CONTROLS.md), [업데이트 보고](05_references/PROTOTYPE_FBX_UPDATE_RECORD.md) |
| `archive/` | 대체된 수치·과거 구현 시점·기획 원문 | [구 전투 수치](archive/balance/COMBAT_BALANCE.md), [구 캠페인 분석](archive/analysis/CAMPAIGN_SIMULATION.md) |

## 문서가 늘어날 때

- **규칙의 원본은 담당 명세 한 곳**에 둔다. 영웅 전용 수치/스킬은 영웅 문서, 수리 전용 견적/속도는 수리 문서가 관리한다. 숫자가 있다는 이유만으로 본문과 수치표를 중복 분리하지 않는다. 여러 분야를 묶는 밸런스 기준표만 `02_balance/`에 둔다.
- 분야 폴더에 들어 있어도 본문에서 **사용자 확정 / 제안 / 미검증**을 구분한다. 분석 폴더의 통계는 해당 입력/버전에만 유효하며 최신 설계의 통과 증거가 아니다.
- 새 확정은 담당 명세를 고치고 결정/R목록/요약의 참조를 갱신한다. 같은 규칙을 다른 문서 끝에 추가 선언하지 않는다.
- 대체된 원문은 Git 이력 또는 `archive/`로 보존한다. 과거 결과/입력의 수치를 새 규칙에 맞춰 고쳐 쓰지 않는다. 기존 archive의 날짜 파일은 당시 원문이다.
- `reference/`는 저장소 루트에 유지한다. 재현 코드·JSON·원시 결과·이미지의 경로와 해시를 보존하며 기획 문서 경로만 연결한다. 영웅/수리 기본값은 [v0.5](../reference/DesignBaseline/README.md), D116을 적용한 통합 시험 스냅샷은 [v0.6](../reference/IntegratedCombat/README.md)다.

## 기존 경로와 작업 재개

이전 `docs/파일명.md` 경로는 [경로 대응표](DOCUMENT_PATHS.json)의 `moved`에서 새 경로를 찾는다. 파일명·D/R 번호를 유지하고 내부 링크와 보고서 생성 도구의 출력 경로를 함께 바꿨다. 루트 `README.md`와 `docs/DESIGN_INDEX.md`는 계속 입구로 사용한다. 고정된 GitHub 과거 커밋/코드 브랜치의 문서 URL은 당시 자료이므로 유지한다.

다음 로컬 작업에서는 최신 `work`의 [문서 지도](DESIGN_INDEX.md) → 담당 명세 → 관련 R목록 → 개발 상태를 읽고 코드 브랜치에 필요한 기획 변경만 반영한다. 폴더 정리가 코드 브랜치의 구현/검증 완료를 뜻하지 않는다.

문서를 옮기거나 링크를 추가한 뒤 저장소 루트에서 `python tools/check_docs.py`를 실행한다. [문서 검사 도구](../tools/check_docs.py)는 로컬 링크/R앵커·경로 대응표·보고서 생성기의 출력 경로를 확인하며 전투 모형이나 UE를 실행하지 않는다. 영웅/수리 수치의 산술 검사는 `python reference/DesignBaseline/validate_design.py`로 별도 실행한다.
