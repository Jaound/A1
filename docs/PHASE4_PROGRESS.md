# 개발 현황·카탈로그·영웅 운용 확장 진행

## 최신 상태 — 2026-11-17, 구현·빌드/API 검증 완료 / 직접 입력 미검증

- 브랜치 `codex/phase1-playground`. 시작 `cadf3affc6f23217ce083a2febac0ab3ffca9934`, 구현 `261159c9e65f7bc0c3c19e0a6ee5d388e70e2240`. 단계별 `4b9c9dc`→`f9227de`→`261159c`를 푸시했다. 문서/검증 메타데이터 최종 제출은 이 구현 이후다.
- 기획 `035a4fba81d26f1d67daa1f44bded0f7e8d3a434`: work fetch 직후 `.git/ov-phase4-planning-sha`에 고정했으며 시작 기준과 변경 없음. work에는 게임 코드를 반영하지 않는다.
- 최종 B9 빌드18.76초·자동80/80. 기존 API57/79/77·선내36·전체통합29사례316검사 통과. 새 개발96·함종270·명단70·강습33·필터96·반복33·보유임무17 통과.79에는 기존2B/R1 검사가 포함되므로 중복 합산하지 않는다.
- 실제 별도창1920×1080, DX11,60초씩 임무1배속/9척320기 기록3배속 측정 완료. 임무평균6.705ms, 편대 교전평균7.965ms. 전체·p95·최대·초기전환·메모리 범위는 검증 문서에 있다. 새10분 연속 실행/200·300코스트 보장 아님.
- UI 실제960×602/1920×1042의5탭 캡처 완료. OS 화면 제어2회 timeout, 키·마우스 입력은 수행하지 않았다. 직접 입력 및 조작감 합격 판정은 남아 있다.

## 구현과 수정

1. 실제 후보 데이터49개+기획상 미정 특수무장 계열1개 카탈로그. 기획/데이터/생성/역할/자동/직접 상태를 독립 표시하고 원본·과거 검사 SHA를 노출한다.
2. Insert의 UMG 통합 현황/함선·초소형함/영웅/무장, 검색·체급/역할/상태 필터·긴 목록 스크롤·상세·닫기. 기존 HUD 전면 교체 없음.
3. 16종 실제 생성·역할 등록·안전 삭제. 초대형 혼합T3/T4와 무장별 유폭. 유한 채굴→수송→시간 기반 선체 수리.
4. 영속 영웅 ID·세력 전체15명·후보레벨/XP/16스킬·유한 소모품·실제 보직 효과·명시 강습 편성·방어·원수단 철수·비전투 저장/불러오기. 보유 명단과 예시 임무 영웅을 구분한다.
5. 보유 영웅으로 시작하는 임무 및 실제 참여/시설 기여/승패에 따른 중복 방지 경험치. DEV 공급/XP와 정상 결과 XP를 분리한다.
6. F6 기존 함선의 선내 등록 누락, 실제 적함 진입 그래프, 미발사 포드 취소/동일ID 재사용, 장면 전환 기록 재활성화, 수호 명령의 자동 점령 오류를 수정했다.

## 남은 제한 / 정확한 다음 작업

- [PHASE4_GUIDE.md](PHASE4_GUIDE.md)의5~10분 직접 확인. 선택/XYZ·창 입력 소비·포커스·영웅 편성/강습/목표/철수·성장을 사용자 입력으로 확인하고 그 결과를 별도 기록한다.
- 새 바이너리의10분 연속 실행·고레벨 다수 영웅·여러 개발 창 최대 부하는 미검증. 기존 PHASE3 600초는 이전 코드 결과다.
- PD 종류 직접 교체 UI, 시설/장갑/무장 전체 수리, 연구·아카데미·정상 캠페인 영입, 구조/포로 캠페인과 최종 그래픽은 미지원이다. 영웅 UI가 있다는 이유로 이 콘텐츠까지 완료라고 하지 않는다.
- 세부 수치는 D114/v0.5와 D116 후보이며 확정 밸런스로 바꾸지 않았다. 시험 공급·원가·명단 생성은 명시 DEV다.
- 현재 상태를 재개할 때 먼저 Git HEAD/status 및 이 문서와 Source를 대조한다. 완료한 시스템을 재작성/초기화하지 않는다.

## 증거와 실행

[검증/한계](PHASE4_VALIDATION.md), [실행·클릭 순서](PHASE4_GUIDE.md), [최종 검사 명세/해시](validation/Phase4/B9-Manifest.json).

주요 파일: `OVDevelopmentCatalogue/Subsystem/Widget`, `OVHeroSubsystem/Widget`, `OVInteriorSubsystem/Tests`, `OVLogisticsSubsystem`, `OVCalibration*`, `OVController*`, `OVHUD*`, `OVMissionSubsystem`, `OVTransportSubsystem`. 증거/도구 외 Content 바이너리와 기존 별도 프로젝트는 변경하지 않았다.

```powershell
Set-Location 'C:\Users\user\source\repos\A1'
& Unreal/OrbitalVanguard/Tools/Build.ps1
& Unreal/OrbitalVanguard/Tools/RunMission.ps1 -Mode Attack
# 재검사: 기존 이름 대신 고유 Label 사용
& Unreal/OrbitalVanguard/Tools/RunPhase4Validation.ps1 -Label NewRun -Automation -Capture -FullMission
```

## 완료 이력

- A1/A2 컴파일 오류와 A3 XP 기대값 산술 오류를 수정했고 A4 빌드/자동73 및 기존 회귀를 통과했다.
- B2 성장/지원/보유임무 구현, 자동80과 신규API를 통과해 `f9227de`로 보존했다.
- B3/B4 UMG 폰트 API 오류 수정, B5/B6 장면 기록 정리/화면 보완, B7 기존 F6 함선 배치, B8 명시 강습, B9 필터/수호 명령 검증을 완료했다.
- Python의 읽기전용 속성·FGuid 표현·비공개 위젯/에디터 설정 접근 오류는 공개 조회/명령 API 및 구조 필드 비교로 수정했다. 생산 규칙 우회로 검사를 통과시키지 않았다.
