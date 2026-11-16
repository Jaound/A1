# IntegratedCombat v0.6 — D116

[시험 기준](../../docs/02_balance/INTEGRATED_COMBAT_BALANCE.md) · [결과](../../docs/04_analysis/INTEGRATED_COMBAT_RESULTS.md)

UE 코드가 아닌 Python3.10+ 설계 모형이다. `profile.json`은 과거 함체/무장·v0.5 영웅/수리와 D116의 새 조건을 고정한 독립 입력이다. `model.py`는 탐지된 접촉 정보만으로 표적을 고르고 XYZ 거리/비행·시설/인원·수리·개별 운송·실제 방/층 경로 기반 선내전을 연결한다. `indoor.py`는 살아 있는 실제 유닛의 위치/레벨/스킬/소모품을 계산하며 과거 승률 표본을 가져오지 않는다.

```bash
python -X utf8 -m unittest discover -s reference/IntegratedCombat -p 'test_*.py' -v
python -X utf8 reference/IntegratedCombat/run.py --explore --workers 2
python -X utf8 reference/IntegratedCombat/run.py --workers 2
python -X utf8 reference/IntegratedCombat/continuation.py
python -X utf8 reference/IntegratedCombat/supplement.py
# 선택: numpy / matplotlib 필요
python -X utf8 reference/IntegratedCombat/plot.py
python -X utf8 reference/IntegratedCombat/report.py
python -X utf8 reference/IntegratedCombat/verify.py
```

탐색은610000번대, 평가810000번대로 구분한다. 탐색 자료는 최종 표본에 합산하지 않는다. `--only 이름일부 --n 횟수 --output 별도파일.json`으로 일부 조건을 별도 실행할 수 있다. 기본 평가에는 동등 코스트/함종 편성, 개인 포드/강습선/사격 보류, 선내 영웅 수/레벨/장비/집중 유형, 함재기 연구/개조, 비대칭 임무, 무장 조준, 시간 간격/순서 민감도가 있다.

네 경계 오류를 수정한 현행 평가는 함대/연동512회 재실행 + 영향 없는 독립 선내816회 재사용이다. 재사용 실행은 `python -X utf8 reference/IntegratedCombat/run.py --workers 2 --reuse-indoor reference/IntegratedCombat/archive/results.json.before_review.gz`다. `dependencies.py`가 원본 해시와 코드/초기화 함수 동일성을 증명한다. 재사용하지 않는 기본 명령은 전체1328회를 새로 실행한다. 어느 쪽도 수정 전/후 중복 시행을 표본에 합산하지 않는다.

`continuation.py`는920000번대·16개 연속2회 시나리오, `supplement.py`는930000번대·40개 배치 한도/좌측 무장 명령 비교다. 보조 결과는 별도 입력/코드 해시를 기록한다. 각 조건의 의미/모형 한계는 결과 보고서를 따른다.

`results.json`은 시드·사례 정의·코드/프로필 해시·모든 시행과 요약을 포함한다. 고정 입력 평가와 코드 변경을 동시에 하지 않는다. 기존 결과와 비교할 때 버전/조건·미해결 비율·표본수·신뢰구간을 함께 읽는다. 개발용 전체 편성/영웅/기체/화물은 **실험의 입력 공개**이며 실제 상대AI/플레이어가 얻는 정보가 아니다. `knowledge`에는 그 정보를 넣지 않는다.

`layouts.json`은 임시 대표 배치를 내보낸 자료다. 모형은 같은 `Layout` 공식/시설 배치로 생성한다. 에셋 기반 최종3D 레이아웃이나UE DataAsset이 아니다. 함재기 개체별 궤도·메시 광선/사선·문 봉쇄·전 경로의 비상탈출/포로 확보·기지전/전체 캠페인 경제는 미지원/근사 범위다. 통계가 이 구현 생략을 인증하지 않는다.

Windows의 기본 문자 인코딩과 무관하게 한국어JSON/문서를 읽도록 위 명령은 `-X utf8`을 명시한다. 기본 모형/검사는 표준 라이브러리만 필요하며 그래프만 numpy/matplotlib을 사용한다.

강습선 귀환 경로 보정의 선택 재실행은 `python -X utf8 reference/IntegratedCombat/refine.py`다. `archive/post_guard_review`와 AST 변경 범위를 확인하고 해당13조건104회를 다시 계산한다. 전체 재실행 명령은 같은 최신 규칙으로1328회를 모두 계산한다.

[검증 기록](VALIDATION.md)에서 검사36개와 단계별 재실행/재사용·남은 한계를 확인한다.
