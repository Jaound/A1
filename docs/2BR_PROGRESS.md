# 2B-R v1 / D118 재개 기록

기획 SHA: `035a4fba81d26f1d67daa1f44bded0f7e8d3a434`. origin/work fetch 직후 고정하고 그 SHA의 PHASE2B_REVISION_PROMPT/PHASE2_PLAN/IMPLEMENTATION_STATUS 및 DESIGN_INDEX를 git show로 읽었다. 이후 코드 fetch와 무관하게 이 SHA를 사용한다. 시작 코드 `ca6e8e5`, 브랜치 `codex/phase1-playground`, 시작 미커밋 변경 없음. 디스크 AGENTS/agent.md를 찾지 못했고 대화의 CODEX DEVELOPMENT RULES를 적용했다.

## 상태와 차이표

| 단위 | 상태 |
| --- | --- |
| R0 동기화 | 완료. `7e30c42`. README 충돌에서 최신 기획과 로컬 실행 링크 모두 보존, 이동된 PHASE2_PLAN은 이전 경로 안내로 유지 |
| R1 쉴드·인원 | 구현·빌드·자동 검사 완료. PIE 회귀 결과는 아래 별도 기록 |
| R1 목적 명령·보호 | 미구현. 기존 2B의 외장 무장 충돌 피해 기반만 존재 |
| R2 데이터·공간 | 미구현. 기존 소규모 ControlRange 수치를 최신 교정 데이터로 오인하지 말 것 |
| R3 전체 인수 | 미완료. 아래 결과는 이번 쉴드 단위의 회귀이며 전체 2B-R 성공이 아님 |
| 후속 제외 | 실제 영웅/강습/PD/함재기/기지·경제/환경4종 미구현 유지 |

구현 체크포인트: `e4a0a36` (쉴드·부분 인원 피해).

## 이번 구현

- 모든 프로필에서 레거시 ShieldRecoveryDelay를 무효화했다. 피격은 회복 시작/진행을 지연하지 않는다. 활성 회복0, 비활성50% 재가동 즉시 정지는 유지한다.
- 후보 기본값은 0.006/0.018 매 게임 초, 비전투 전환15초. 고정 최종 밸런스가 아니다.
- 가동 제어실의 명목 용량과 실제 인원 계수를 분리했다. 단일실 부분 인원 손실은 최대량을 줄이지 않고 회복에만 한 번 적용한다. 다중실 최대량은 `AdditionalShieldCapacity=.5` 시험 설정으로 기존 중복 형태 유지. 회복 인원비는 실제 시설 효율/동수 완전 인원 효율이다.
- 실제 발사/피격은 NotifyCombatParticipation으로 전투 시계를 갱신한다. TimeSinceExternalHit는 호환을 위해 이름을 유지한 전투 참여 경과시간이다. DEV ApplyWorldHit 기본 호출은 전투 참여로 세지 않는다.
- ApplyWorldHit/ApplyWorldWeaponHit의 선택 매개변수 bCombatAttack을 실제 CombatSubsystem만 true로 전달한다. 실제 시설 피해(연쇄 포함)는 `필요 인원 × clamp(전달 피해/시설 최대 HP,0,1) × CrewDamageCoefficient(.20)`만큼 부분 손실시킨다. 이 선형 곡선은 후보다. HP/인원/점령/무장 HP는 별도이고 물리 수리는 인원을 생성하지 않는다.

## 검사와 로그

Win64 Development Editor 최초30.17초, 최종4.96초 빌드 성공. MSVC14.51.36260 / SDK10.0.26100.0. 자동 **32/32**, 기존31개 ID 유지 + `Systems.D116NoDelayCrewAndDamage`. 기존 ShieldStateAndTime 기대값을 D116 무지연으로 수정했다. 한국어 엔진 자체 검사15건과 권장 MSVC/VS SDK 경고는 별도 유지한다.

첫 새 테스트는 무인 코어에 명중시키는 fixture여서 인원 감소 검증이 실패했다. 실제 장갑→유인 슬롯 매핑으로 시험 위치를 고친 뒤 전체32개가 통과했다. 테스트를 삭제하거나 인원 감소 요구를 약화하지 않았다.

원본: 프로젝트 Saved/Logs/RevisionR1ShieldBuild.log, RevisionR1ShieldFinalBuild.log, RevisionR1ShieldAutomation.log(초기 실패), RevisionR1ShieldAutomationFinal.log(최종 성공). 직접 OS 입력은 이번 단위에서 실행하지 않았다. 이전 캡처 실패를 이번 재시도 결과라고 쓰지 않는다.

## 다음 작업 (전체 작업은 아직 미완료)

1. `OVCombatSubsystem`에 공격 의도/명령 상태 추가: 선체 격파 vs 관측 무장 무력화. 관측 장착점이 없으면 완료/대기, 실제 UpdateAim/Launch/Sweep 경로를 공유하고 선체로 자동 전환하지 않는다.
2. 세력별 내부 잔류/목표 완료/생존자 인터페이스와 DEV 입력, 세 정책(기본 내부 이탈까지), 명시 강제 무장 공격의 명령·표적·공격 무장 범위를 구현한다. 자동 재선정/잔여 살보/나포·취소의 상태 정리와 기발사 유지 검사 추가. 현 단계는 이 인터페이스가 없다.
3. R2: reference/IntegratedCombat/profile.json 선택 필드를 읽어 Calibration_D116_v06 데이터/출처 해시/미지원 목록 작성. ships/weapons/radar/tier_damage/integration만 선별. global/Python radius·집계 확률·승률·가짜 영웅을 복사하지 않는다. m→cm1회, 외형/장착/슬롯/충돌/카메라/명령 범위를 함께 교정. Vector/Bastion 구분과 초대형 화력 유지.
4. R3: 기존32/2A57/2B50 및 추가 명령/보호/공간/시간/실제 충돌 검사. 소수 편대 비교·입력 체크리스트·새 가이드/검증 문서 작성. 기존 결과는 이전 정책 증거로 보존한다.

재실행은 기존 PHASE2B_GUIDE의 Build.ps1/자동 검사/ValidatePhase2A.py/ValidatePhase2B.py 명령을 사용한다. 최신 계획은 docs/03_development/PHASE2_PLAN.md. 저장소와 기존 SmokeTest는 보존했으며 코드 브랜치만 제출한다.

기존 2A PIE API57/57 통과(2026-11-16 17:15:51 KST). 보고서: `validation/Phase2BR-R1-2A-PIE.json`. API 성공은 직접 입력 성공이 아니다.

기존 2B PIE API50/50 통과(2026-11-16 17:17:27 KST), 오류0. 보고서 `validation/Phase2BR-R1-2B-PIE.json`. 새 2B PIE를 일시정지 상태로 남겼다. 새 명령/보호 기능은 아직 없어 이50개가 그 기능을 검증한 것은 아니다. JSON의 기존 기획 메타데이터는 역사적 검사 스크립트의 값이며 이번 실행 소스는 e4a0a36이다.

직접 확인: 기존 Tools/RunEditor.ps1 → Play → F5 → T로 표적 지정 → B → F1. 쉴드 파괴 후 DEV 표시에서 비활성 게이지가 즉시 증가하는지 확인한다. 기존 이동/사격 독립 자동화는 유지된다. 새로운 무장 목적 명령/강습 보호 버튼은 아직 제공하지 않는다.

재개 시 우선 Git HEAD/미커밋 변경과 이 기록을 확인하고, 완료된 쉴드 단위를 다시 만들지 말고 R1 명령/보호에서 이어간다. 전체 완료 뒤의 R3 최종 보고와 교정 프로필 비교는 남아 있다.
