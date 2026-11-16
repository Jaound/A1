# FleetEndurance v0.3

[수치 명세](../../docs/FLEET_ENDURANCE_BALANCE.md) · [결과](../../docs/FLEET_ENDURANCE_RESULTS.md)

Python3.10+ 표준 라이브러리만으로 코스트100/200/300과 전 함종을 시험하는 **집계 전투 모형**이다. XYZ 물리/함선 내부 전체 시뮬레이터가 아니다. 작은 함선용`CombatSimulation`과 별도 버전으로 보존한다.

```bash
python -m unittest discover -s reference/FleetEndurance -p 'test_*.py' -v
python reference/FleetEndurance/run.py --n 24 --seed 50000
python reference/FleetEndurance/sensitivity.py
python reference/FleetEndurance/report.py
```

- `profile.json`: 현행 후보·실험 목표·16종 함체·코스트·종류별 무장 참조·실험 계수.
- `model.py`: 지속 포격·지연 공격·소형 표적 추적·쉴드·유한 수리·함재기/PD/공중전·포드/강습선·선내 표본·점령/반격·귀환.
- `run.py`: 38조건×24시드. 구 HP/쉴드만 적용한 대조군도 새 모형 안에서 실행한다. 이전v0.2 결과 자체를 재실행한 것은 아니다.
- `results.json`: 원시 시행·요약·각 코스트별 정확한 함종/척수와 코드/수치/선내 의존성 해시.
- `sensitivity.json`: 별도 시드80000~80002, 1/0.5초 비교. 수렴 인증이 아니다.
- `test_model.py`: 코스트/모든 함종 포함·쉴드·요격/수리·기발사 공격·기체 손실/나포 결과 검사.
- `report.py`: 결과표 생성. 문서 해석/한계는 새 수치나 시나리오로 변경할 때 함께 재검토한다.

강습선·수송선의 소형 코스트와 운용·관통 배율·PD 국소 그룹은 시험 제안이다. 자동 분산 표적 선호를 사용자 집중 사격의 하드 제한으로 이식하지 않는다. 전투 시간은 게임 시간이며 실제10~15분 게임 플레이/영웅 구조·예비대/장기 캠페인 검증이 아니다.

최종v0.3c는 전투 소형HP6배·포드160HP다. 주 시드50000번대/강습70000번대, 동일 시드 대조72회·해상도18회·소형 대응 반례48회는 별도 파일로 보존한다. a/b는archive의이전 후보이며 유효 표본에 합산하지 않는다.

```bash
python reference/FleetEndurance/paired_controls.py
python reference/FleetEndurance/counter_checks.py
python reference/FleetEndurance/verify.py
# 선택: matplotlib이 설치된 환경
python reference/FleetEndurance/plot.py
```

검증 기준은`results.json`의해시/입력이다. 수치 탐색 파일은 현재 검증 표본에서 제외한다. 새 수치를 시험할 때 기존 결과를 다른 파일에 보존하고 시드를 바꿔 재평가한다.

`defense_checks.py`는 선내 방어224회 진단이며 기본 연동군(병사8명)과 분리한다. 함대 연동 결과에 방어 영웅을 소급 적용하지 않는다.

[검증 기록](VALIDATION.md) · `route_checks.py`: 내부/복귀 이동3배의36회 스트레스 진단. `archive/model_before_radar_cache.txt`는 최적화 전 소스 스냅샷이며 직접 실행하는 엔트리포인트가 아니다.
