# 우주 환경 단독 전투 교정 — E1 v0.1

운석·방사능·플레어·블랙홀을 **한 전투에 하나씩만** 적용한 Python 설계 모형이다. 실제 UE 실행·최종 모델의 충돌/노출·완성 게임 밸런스를 인증하지 않는다.

- [권장 수치와 단위](../../docs/02_balance/SPACE_ENVIRONMENT_BALANCE.md)
- [비교 결과와 한계](../../docs/04_analysis/SPACE_ENVIRONMENT_RESULTS.md)
- [UE 구현용 초기 프로필](implementation_profile.json)
- [최종140개 고유 조건/시드](final_results.json)
- [CSV 요약](final_summary.csv)
- [파일 해시 목록](environment_manifest.json)

## 표본을 구분해서 읽기

| 자료 | 범위 | 집계 방식 |
| --- | --- | --- |
| pilot_results.json | 약/기준/강 입력28회·600초 | 탐색, 최종 승률에 합산하지 않음 |
| evaluation_results.json | 초기120회 비교, 플레어R120·상시엄폐 포함 | 이전 기준 비교. 최종140과 중복 합산 금지 |
| final_flare_results.json | 새 시드의 플레어R60·위험도 대응20회 + 일반/엄폐 대조40회 | 최종 자료에60행 반영 |
| final_results.json | 환경80 + 일반대조40 + 엄폐대조20 =140 | 최종 E1의 고유 표본 |
| sensitivity_results.json | 준비/무방비·저항·시간간격30회 | 별도 민감도 |
| boundary_results.json | 노출/엄폐/무동력/FTL49개 에피소드 | 승률 표본이 아님 |
| asteroid_persistence_results.json | 운석HP별 제거 시간12회 | 지형 유지 시간 교정 |
| flare_policy_results.json | 같은 조건 정책 재검사10 + 새 시드 쌍대8 | 추가 정책 비교18회 |
| flare_cover_results.json | R60/80/100m 엄폐 교정12회 | 선택용 탐색 |
| flare_stop_results.json | 정지 길이의 고립 요격 구간10,800회 | 초기 D124 시험, 함대전 표본과 별도 |

주 평가의 각 환경·편성은4시드다. 전체 승률이나 모든 메타의 균형을 확정하는 표본 수가 아니다. 본 전투는 자동 퇴각을 끄고 최대1800게임초까지 관측한다. 시간 제한·일시 마비·교착을 승리나 영웅 사망으로 바꾸지 않는다.

## 선택한 최종 구성

- 원형 입력은 IntegratedCombat v0.6이며 과거 파일을 수정하지 않았다.
- 프로세스 내부에서 항공순양함T4전용주포2문, 격납고 상한 항모4/항공순양함8과 실제2/4를 구분한다.
- 강습선의 무료 경비를 제거하고 강습조는 영웅4+병력6으로 맞춘다.
- 중형 재산정 전 수송선은 이 편성에 포함하지 않는다.
- 초기120회 플레어는R120 엄폐물·상시엄폐였다. 교착이 많아 다른 시드로R60/80/100과 위험도 대응을 탐색했다.
- 최종 플레어는R60·위험도 대응, 새 시드178100~178103이다. 나머지 환경은173100~173103이며 각각 같은 시드의 자체 대조군과 비교한다.
- 일반 엄폐 구조물은 플레어 시험의 지형이다. 동시 운석 환경의 충돌 피해·파괴를 켠 것이 아니다.

## 재현

Python 표준 라이브러리만 필요하다. 저장소 루트에서 실행한다. 워커는 독립 모형을 계산하며 각 배틀의 환경 종류는 하나로 제한한다. 주요 스윕은 환경 종류별 배치를 순서대로 실행한다.

    python -X utf8 -m unittest discover -s reference/SpaceEnvironmentCalibration -p 'test_environments.py' -v
    python -X utf8 -m unittest discover -s reference/IntegratedCombat -p 'test_rules.py' -v
    python -X utf8 reference/SpaceEnvironmentCalibration/run_environment_sweep.py --stage pilot --output /tmp/E1-pilot-replay.json
    python -X utf8 reference/SpaceEnvironmentCalibration/run_environment_sweep.py --stage evaluation --profiles reference/SpaceEnvironmentCalibration/benchmark_profiles.json --output /tmp/E0-reference-replay.json
    python -X utf8 reference/SpaceEnvironmentCalibration/run_final_flare.py
    python -X utf8 reference/SpaceEnvironmentCalibration/report_environment_results.py

Windows에서는 /tmp 대신 실제 임시 폴더 경로를 사용한다. 일부 명령은 지정된 결과 파일을 다시 쓴다. 검증 자료를 보존하려면 별도 체크아웃에서 재현하고 원본 증거와 비교한다.

현재 selected_profiles.json은R60·위험도 대응의 E1이고 benchmark_profiles.json은 이전R120 기준 입력이다. 이전120회 재현에는 benchmark_profiles.json을 명시해야 한다.

기존 민감도30회는R120 입력으로 수행했다. 재현 시 같은 입력을 지정한다.

    python -X utf8 reference/SpaceEnvironmentCalibration/run_environment_sweep.py --stage sensitivity --profiles reference/SpaceEnvironmentCalibration/benchmark_profiles.json --output /tmp/E0-sensitivity-replay.json
    python -X utf8 reference/SpaceEnvironmentCalibration/probe_environment_boundaries.py
    python -X utf8 reference/SpaceEnvironmentCalibration/probe_asteroid_persistence.py

추가 정책/엄폐 교정은 실제 수행 당시 선택 프로필과 코드 해시를 결과 파일에서 확인한다. 최종 E1 파일로 실행하면 입력이 달라질 수 있으므로 과거 자료를 덮어쓰며 동일 실험이라고 부르지 않는다.

[초기 정지 시간 시험의 원래 안내](FLARE_STOP_SENSITIVITY.md)는 별도로 보존했다.

## 의존성·실패 이력

- 초기 엄폐물과 항모가 겹치는 배치를 발견했다. evaluation_invalid_shelter_layout.json의 지형 관련 행은 최종 통계에서 제외했다.
- 영향 없는 일반 대조·운석·방사능60행만 AST/입력 해시 증명을 거쳐 초기120회에 재사용했다. 잘못된 지형 행이나 미완료 결과를 성공 표본으로 채우지 않았다.
- 당시 재사용 증명은 reuse_proof.json과 코드 스냅샷eac4063/1bd1691에서 확인한다. 현재 코드는 추가 선택적 AI 분기를 포함하므로 위 전체 재실행 명령을 권한다.
- pilot_before_shelter_los_fix.json, pilot_before_braking_fix.json, pilot_invalid_shelter_layout.json은 수정 전 탐색 이력이며 최종 표본이 아니다.
- 최종140행은 초기120 중 영향 없는80행 + 새 플레어/대조60행이다. 이전 연구와 합산해 표본 수를 늘리지 않는다.
- 본평가 소스 스냅샷eac4063과 최종 코드의 선택적 cover_policy 분기는 구분한다. 실제 실행 파일 SHA-256은 각 결과와 environment_manifest.json에 기록한다.

## 남은 모델 한계

함체/장갑/시설은 임시 논리형상이며 함재기 공격·PD 일부는 집계 근사다. 개체별 환경 위치도 프록시 궤적을 사용한다. 함대 충돌·엄폐점 점유·최종 메시 광선·실제 추진 성능·생명유지/FTL의 EMP 상호작용·전용 인양·기지/채굴 보상은 추가 검증이 필요하다.

초기 구현에는 implementation_profile.json을 참고하되 UE 데이터와 단위를 대조한다. selected_profiles.json의 옛 avoid_margin 후보키처럼 실제 계산에 사용하지 않는 값은 그대로 이식하지 않는다. 환경 없는 기존 D116 결과도 수정하지 않는다.
