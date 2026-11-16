# 2B-R v1 / D118 재개 기록

## 2026-11-16 R2·R3 체크포인트

기획 `035a4fba81d26f1d67daa1f44bded0f7e8d3a434`, 검증 코드 `329518e`. D116 v0.6 후보 이식과 F6 교정 장면, 공간 범위, R3 교전 비교 및 기존 회귀를 완료했다. Win64 Editor 빌드 성공, 자동 **36/36**, 2A API **57/57**, 종합 API **79/79**. 종합 API 수에 기존 2B와 R1 명령 검사를 포함한다. 직접 입력은 캡처 재시도 시간 초과로 미검증이다. 다음 확인은 F6/F5 실제 입력·조작감이다. 데이터 해시/매핑, 남은 제약, 검증 로그, 실행법은 [D116 교정 가이드](PHASE2BR_D116_CALIBRATION.md). 아래는 이전 단계 증거다.

## 최신 체크포인트: R1 명령·보호 (2026-11-16)

구현 `8228f04`. 시작 HEAD `0bf763b`, 미커밋 변경 없음. work fetch 직후 고정한 SHA는 그대로 `035a4fba81d26f1d67daa1f44bded0f7e8d3a434`이며 새 기획 변경 없음. R0와 쉴드 구현을 재작성하지 않았다.

완료: 선체/무장 목적, 관측 장착점에 대한 기존 조준·탄도·충돌 경로, 관측 무장 없음 대기, 세력별 내부 잔류/목표 완료 인터페이스, 세 보호 정책, 지정 공격 무장만의 강제 예외, 취소/표적 변경/자동화 변경/나포의 미발사·예외 정리. 기발사 공격의 소속/탄도를 변경하지 않는다. 소유 세력 생존자 입력으로 승무원0 중립화 경계를 연결하되 시설을 무료 가동하지 않는다. 실제 영웅/강습은 구현하지 않았다.

최종 빌드6.81초 성공. 자동35/35(기존32+Revision3), PIE60/60(기존2B50+새 명령10), 2A PIE57/57 통과. 증거는 `validation/Phase2BR-Orders-*`, 원본 로그는 `Saved/Logs/RevisionR1Orders*.log`. OS 마우스/키보드는 이번에 수행하지 않았다. 한국어 엔진 자체 검사15건은 별도다.

현재 범위의 한계: 임시 관측 모델은 유효 탐지 시 외장 장착점을 공개하며 개별 메시 가림/시야 판정은 없다. 시설/영웅 로스터를 관측 정보로 공개하지 않는다. 무장 선택은 관측된 살아 있는 첫 장착점부터이며 자동 선체 공격으로 전환하지 않는다. FTL 충전의 효율0 취소는 기존 시험 동작을 유지하며 물리 파괴와 인원/점령 사유를 통합된 최종 정책으로 확정하지 않았다.

다음은 **R2**다. profile.json의16종 ships/7종 weapons/radar/tier_damage/integration 구조만 확인했으며 `Calibration_D116_v06`/UE 공간 변환은 미구현이다. R3 전체 비교와 새 교정 장면 검증도 남았다. R2에서는 `OVController.cpp`의 줌 범위/평면 ±6000/고도 -1500~4000 및 `OVOrderInput` 제한을 함께 확인하고, 기존 ControlRange 감도는 유지하면서 새 프리셋 공간 범위를 설정화해야 한다. Python radius를 형상으로 쓰지 않는다. 아래는 이전 쉴드 단위 기록이며 그 '다음 작업' 중 R1 명령·보호는 위 구현으로 대체되었다.

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
