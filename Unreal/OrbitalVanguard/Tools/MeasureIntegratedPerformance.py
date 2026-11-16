"""실제 렌더 진단기의 CSV/상태 시계열을 요약한다. UE나 전역 설정을 바꾸지 않는다."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics


def metrics(values):
    ordered = sorted(values)
    if not ordered:
        return None
    return {"표본": len(ordered), "평균": statistics.mean(ordered),
            "p95": ordered[max(0, math.ceil(len(ordered) * .95) - 1)], "최대": ordered[-1]}


def summarize(folder):
    result = {"설명": "CSV 비용은 ms. Exclusive 항목은 부모 구간의 자식 비용을 제외한다. GameThread/GPU 총계와 Exclusive 항목을 중복 합산하지 않는다.",
              "프로파일전체": {}, "구간별프로파일": {}, "메모리와객체": {}, "경고": []}
    profile = folder / "프로파일.csv"
    if profile.exists():
        with profile.open(encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            names = [name for name in reader.fieldnames if name in ("FrameTime", "GameThreadTime", "RenderThreadTime", "GPUTime") or "/OV_" in name]
            columns = {name: [] for name in names}
            stages = {}
            for row in reader:
                try:
                    case = int(float(row.get("OrbitalVanguard/CaseIndex", "")))
                    stage = int(float(row.get("OrbitalVanguard/StageIndex", "")))
                except (TypeError, ValueError):
                    case, stage = -1, -1
                group = stages.setdefault(f"사례{case}_단계{stage}", {name: [] for name in names})
                for name in names:
                    try:
                        value = float(row[name])
                        if math.isfinite(value):
                            columns[name].append(value)
                            group[name].append(value)
                    except (TypeError, ValueError):
                        pass
            result["프로파일전체"] = {name: metrics(values) for name, values in columns.items()}
            result["구간별프로파일"] = {key: {name: metrics(values) for name, values in group.items()} for key, group in stages.items()}
            if not any("/OV_" in name for name in names):
                result["경고"].append("함재기/요격/충돌/UI의 개별 계측이 CSV에 없습니다. 전체 프레임만으로 병목을 확정하지 마세요.")
    else:
        result["경고"].append("프로파일 CSV 없음. 실행 로그와 CSV 저장 완료 여부를 확인하세요.")
    history = folder / "상태시계열.csv"
    if history.exists():
        with history.open(encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream))
        for key in ("UObject추정개수", "프로세스물리MB", "진입기록", "기체피격기록", "요격기록", "대함피격기록", "기체레코드", "기체탄", "대함탄", "승객", "포드"):
            values = [float(row[key]) for row in rows]
            if values:
                result["메모리와객체"][key] = {"시작": values[0], "끝": values[-1], "최대": max(values), "변화": values[-1]-values[0]}
        result["상태시계열표본"] = len(rows)
    output = folder / "프로파일요약.json"
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"프로파일 요약 저장: {output}")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path, help="성능요약.json과 프로파일.csv가 저장된 진단 폴더")
    summarize(parser.parse_args().folder)
