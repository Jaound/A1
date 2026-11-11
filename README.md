# 오비탈 뱅가드 (가제)

3D 우주 함대전과 2D 선내 전술을 결합한 싱글플레이 지속형 RTS의 작업 저장소입니다.

## 기획 문서

- [게임 기획 요약](docs/GDD.md): 게임의 정체성, 시스템, 플레이 흐름
- [결정 및 미결정 사항](docs/DECISIONS.md): 확정된 방향과 아직 선택해야 할 규칙
- [함대 전투 설계안](docs/FLEET_COMBAT.md): 함급과 전투 역할에 대한 검토용 초안
- [함선 내부 시설](docs/SHIP_SYSTEMS.md): 기본·특화 시설과 파괴 효과
- [영웅·강습 시스템](docs/HERO_SYSTEMS.md): 영웅의 임무·유형, 시설 점령과 함선 지원
- [선내 자동 전투·점령 기본안](docs/BOARDING_COMBAT.md): 경로·교전·점령·후퇴의 첫 검증용 설계
- [UI·UX 시스템](docs/UI_SYSTEMS.md): 플로팅 창, 실시간 3D 뷰어, 입력·HUD·전술 정보
- [프로토타입 통합 개선안](docs/PROTOTYPE_IMPROVEMENT_PLAN.md): 플레이 테스트의 설계·밸런스 개선 기준
- [개발 시행착오](docs/DEVELOPMENT_PITFALLS.md): 재발 방지 사례와 검증할 예외
- [제작 순서와 로컬 개발 안내](docs/IMPLEMENTATION_PLAN.md): 언리얼 프로젝트 생성, 단계별 구현과 검증 기준
- [2단계 제작 계획](docs/PHASE2_PLAN.md): 최신 기획 동기화와 다층 시설·외부 함대전 구현 순서
- [최신 기획 통합 검토](docs/PLANNING_REVIEW.md): 정정 사항, 충돌과 미결정 규칙, 구현 영향
- [UE5 제작 공통 기준](docs/UE5_IMPLEMENTATION_CONTRACT.md): 문서 적용 순서, 좌표·상태·UI 분리와 회귀 검사

새로운 기획을 논의할 때 위 문서를 먼저 확인하고, 합의된 변경을 반영합니다. 제작을 이어갈 때는 통합 검토와 UE5 공통 기준을 읽은 뒤 2단계 계획의 현재 완료 단위를 확인합니다. 프로토타입에서 가져온 API·좌표·상대 수치와 UE5에서 실제 검증한 결과는 구분합니다.

## 실행 가능한 첫 제작 단계

UE 5.8.4 C++ 프로젝트: [`Unreal/OrbitalVanguard/OrbitalVanguard.uproject`](Unreal/OrbitalVanguard/OrbitalVanguard.uproject).

- [실행 방법과 조작법](docs/PLAYGROUND_GUIDE.md): 임시 함선 3척을 사용하는 3D 조작 시험장
- [빌드·에디터·PIE 검증 결과](docs/PLAYGROUND_VALIDATION.md): 자동 검사와 직접 입력 미확인 항목을 구분한 기록
