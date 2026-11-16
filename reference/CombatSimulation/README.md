# CombatSimulation — 전투 수치 검토용 축소 모형

[수치](../../docs/COMBAT_BALANCE.md)와 [결과/한계](../../docs/COMBAT_SIMULATION.md)를 먼저 읽는다. UE 구현이나 최신 Godot Web ZIP을 실행한 프로그램이 아니다. Python 3.10+ 표준 라이브러리만 사용한다.

```bash
python -m unittest discover -s reference/CombatSimulation -p 'test_*.py' -v
python reference/CombatSimulation/verify_results.py
python reference/CombatSimulation/run.py --workers 4
# 간단 재현: 별도 파일에 저장하여 공식 표본을 보존
python reference/CombatSimulation/run.py --workers 2 --quick --output quick_results.json
```

- `profile.json`: 수치·단위·시험 상태 원본. 함선·무장·PD·영웅·점령·보직 초안.
- `engine.py`: 제한된 3D 발사체/부위피해, 1D 선내 전투, 두 모형을 연결한 강습 시계.
- `run.py`: 고정 편성·평가 시드·스텝, 원시 결과와 요약 생성. 병렬 프로세스 수는 결과를 바꾸지 않는다.
- `calibration.json`: 평가 시드와 분리한0~19 영웅 조정 및 초기 함대 탐색, v0.1→v0.2 조정 근거. 정식 반복 결과와 합산하지 않는다.
- `test_invariants.py`: 쉴드/독립 공급/탐지/기발사 공격/영웅 회복·사망 등 모형 자체의 규칙 검사.
- `results.json`: 함대 시드1000부터·영웅/연동4000부터, 편성/원시 시행/95% Wilson 구간/다른 시드의 해상도 비교와 소스 해시.

무작위 시드 반복은 같은 제한된 기하/AI 내의 변동이다. 실제 플레이어·전장·밸런스 전체의 표본으로 해석하지 않는다. 해시가 바뀌면 이전 결과를 새 코드 검증으로 사용하지 말고 새 파일로 다시 실행한다. 실행 시간은 CPU·프로세스 수에 따라 달라진다. 실행 중 설정을 수정하지 않는다.

자동 테스트 통과는 참조 모형의 검사일 뿐 UE 자동/API/빌드 검사 수에 더하지 않는다. 캐리어·함재기·수리함·전장 경로/교통·재탈환·승무원 손실·생명유지·시설 중복·화재·FTL은 이 버전의 교전 평가에서 제외한다. 외장 무장HP 단독 저격도 실제 발사 경로로 검증하지 않았다.

`archive/profile_v01.json`·`archive/results_v01.json`은 과거 후보다. 현행은`profile.json`의v0.2다. 원거리 사거리20→18m 뒤 영웅/연동은 새 시드로 평가했다. 재사용 옵션은 함대 소스 구성요소·기존 프로필 해시와 함대가 읽는 데이터가 같은 경우에만 함대 시행을 재사용한다.

```bash
python reference/CombatSimulation/run.py --workers 4 --reuse-fleet reference/CombatSimulation/archive/results_v01.json --reuse-profile reference/CombatSimulation/archive/profile_v01.json
python reference/CombatSimulation/shield_delay_experiment.py
# 선택: matplotlib 설치 시 그래프 내보내기
python reference/CombatSimulation/plot_results.py
python reference/CombatSimulation/write_report.py
```

`shield_delay_experiment.py`/`shield_delay_results.json`은 별도 시드3000대의8쌍 민감도 비교이며 정책 채택이 아니다. 주 평가1136회와 해상도24회에 이16회를 별도 표시한다.

`write_report.py`는 이 고정 프로필의 문서 생성기다. 새로운 버전으로 수치/편성/평가 수가 바뀌면 해석 문장·한계·검사 수도 함께 검토한다. 그래프/문서의 수치 표는results.json에서 생성한다.

[검증 기록](VALIDATION.md)에 유효 표본·재현·해상도 한계를 정리했다.
