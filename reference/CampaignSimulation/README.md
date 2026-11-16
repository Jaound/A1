# Campaign economy/workload model

이 폴더는 UE 게임 코드가 아니라2026-11-16 기획 검토용 Python 모형이다. [한국어 분석 보고서](../../docs/CAMPAIGN_SIMULATION.md)를 먼저 읽는다. `completion`은 가정된 임무 작업량 충족이며 전투 승리 확률이 아니다.

```bash
python reference/CampaignSimulation/simulate.py --seeds 10 --plan six
python reference/CampaignSimulation/simulate.py --seeds 10 --plan ten
python reference/CampaignSimulation/validate.py
```

Python표준 라이브러리만으로 시뮬레이션/검증한다. PNG는 `python reference/CampaignSimulation/plot.py`(matplotlib필요)로 재생성할 수 있는 정적 분석 산출물이며모형 실행에matplotlib는불필요하다.

- `simulate.py`: 가격/연구/건설/생산·자원·2투자정책·2가상임무안·손실가정. 실제시간0.05분(3초) 간격.
- `validate.py`: 기준문서 비용/시간·자원장부·슬롯·출격명단·재현성 및 결과 검사.
- `results_six.json` / `results_ten.json`: 각180회 최종 실행요약/개별결과, 미리 지정한seed0의 이벤트 기록.
- `trace_six.csv` / `trace_ten.csv`: 균형투자/혼합배속/표준손실/seed0 대표 시계열.
- `resolution_check.json`: 대표seed0의3초/1.5초 시간 간격 비교.
- `campaign_timeline.png`: 대표10임무 진행/재고 그래프.
- `provenance.json`: 기준 기획/모형 해시와 실행환경.

각 실행은300실제분또는작업량완료시종료한다.300분이후완료시간은추정하지않는다. 결과중앙값은완료한실행만의조건부값이며중도절단을확인한다.10개시드는경제손실가정의민감도표본이지플레이어승률표본이아니다. `repair_due_at_end`는종료시아직지불하지않은마지막임무수리견적이다.
