"""통합 PIE 결과의 동일 조건만 비교한다. 제한 시간 사례를 승패로 바꾸지 않는다."""
import argparse
import collections
import json
from pathlib import Path
import statistics


def metrics(values):
    values = [v for v in values if v is not None and v >= 0]
    if not values:
        return {"표본": 0, "평균": None, "최소": None, "최대": None}
    return {"표본": len(values), "평균": round(statistics.mean(values), 3), "최소": round(min(values), 3), "최대": round(max(values), 3)}


def summarize(source):
    raw = json.loads(source.read_text(encoding="utf-8-sig"))
    groups = collections.defaultdict(list)
    for c in raw["사례"]:
        cfg = c["조건"]
        if cfg.get("clock") or cfg["attack_heroes"] == 4:
            continue
        groups[(cfg["mode"], cfg["attack_heroes"], cfg["defense_heroes"])].append(c)
    result = {"출처": source.name, "종류": "동일 편성·초기 수치·복수 시드의 제한된 시험 비교", "최종밸런스확정": False,
              "시간단위": "게임초; API 실행 벽시계와 다름", "군함코스트": "각31; 항모25/바스티온5/강습선1",
              "집계주의": ["함대만의600초 미결정은 승패/전투 완료 시각으로 대체하지 않음", "방어 모드는 플레이어 방향과 실행 순서가 바뀜",
                        "함재기 손실은 현재 소속 기준; 생존 코스트는 원 편성 중 해당 소속 유지 함선", "철수명령 전 전멸은 철수 성공률 분모에 넣지 않고 작전 실패로 함께 기록"], "비교": []}
    for (mode, heroes, guards), cases in groups.items():
        launched = sum(c["launched"] for c in cases)
        entered = sum(c["entered"] for c in cases)
        eligible = sum(c["alive_when_retreat_ordered"] for c in cases)
        returned = sum(c["returned_after_entry"] for c in cases)
        row = {"모드": mode, "공격영웅": heroes, "수비영웅": guards, "시드": [c["조건"]["seed"] for c in cases],
               "결과": dict(collections.Counter(c["result"] for c in cases)),
               "실제완료시간": metrics([c["finished_at"] for c in cases]), "관측종료시간": metrics([c["game_time"] for c in cases]),
               "쉴드제거시간": metrics([c["shield_break_at"] for c in cases]),
               "운송생존율": {"발사": launched, "진입": entered, "비율": entered/launched if launched else None},
               "진입부터쉴드실작전": metrics([c["shield_operation_at"]-c["first_entry_at"] if c["shield_operation_at"] >= 0 and c["first_entry_at"] >= 0 else None for c in cases]),
               "진입부터함교점령": metrics([c["bridge_capture_at"]-c["first_entry_at"] if c["bridge_capture_at"] >= 0 and c["first_entry_at"] >= 0 else None for c in cases]),
               "철수회수율": {"명령시진입생존자": eligible, "원수단회수": returned, "비율": returned/eligible if eligible else None}, "세력별최종손실": []}
        for faction in [0, 1]:
            entries = [next(f for f in c["factions"] if f["faction_id"] == faction) for c in cases]
            row["세력별최종손실"].append({"소속": faction, **{key: metrics([e[key] for e in entries]) for key in
                                            ["ships_lost", "heroes_dead", "heroes_rescue_waiting", "troops_dead", "aircraft_lost", "living_cost"]}})
        result["비교"].append(row)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(json.dumps(summarize(args.source), ensure_ascii=False, indent=2), encoding="utf-8")
    print("통합 비교 요약 저장:", args.output)
