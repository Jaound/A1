"""완료된 실제 렌더 임무의 수명/ID/누적 기록을 요약한다. UE 실행과 원본 변경은 하지 않는다."""
import argparse
import csv
import json
import math
from pathlib import Path
import statistics


def load_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def load_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def limits(rows, key):
    values = [float(row[key]) for row in rows]
    return {"시작": values[0], "끝": values[-1], "최소": min(values),
            "최대": max(values), "변화": values[-1] - values[0]}


def summarize(folder, output):
    frame = load_json(folder / "성능요약.json")
    process = load_json(folder / "프로세스측정.json")
    if not frame.get("완료") or not frame.get("통합임무"):
        raise ValueError("완료된 OVPerfMission 실제 렌더 결과만 요약할 수 있습니다.")
    if process.get("종료코드") != 0 or process.get("시간초과"):
        raise ValueError("진단 프로세스가 정상 종료되지 않았습니다. 원본 로그부터 확인하세요.")
    rows = load_csv(folder / "선내시계열.csv")
    world = load_csv(folder / "상태시계열.csv")
    if not rows or not world:
        raise ValueError("선내/월드 시계열 표본이 없습니다.")
    result = {
        "종류": "1920×1080 별도 게임 창 실제 렌더 수명 검사; 직접 마우스/키보드 검사가 아님",
        "코드SHA": process["코드SHA"],
        "측정시작미커밋변경": process["미커밋변경"],
        "조건": frame["임무조건"],
        "기준": "유닛/개인 초기 ID는 정상 장면 생성 직후 저장. 포드는 실제 발사 때 생성됨. 사망/포획/귀환으로 재생성하지 않음.",
        "원본폴더": str(folder),
        "추적파일": process.get("CPU추적경로"),
        "직접입력": process["직접입력"],
        "요청측정초": frame["요청측정초"],
        "준비포함실제렌더초": frame["전체벽시계초"],
        "프로세스전체벽시계초": process["전체벽시계초"],
        "선내시계열표본": len(rows),
        "프레임구간": frame["구간"],
        "프레임구간집계주의": "구간 코스트 열은 종료 시점의 현재 세력별 합계다. 나포 전 초기 편성 코스트와 같다고 해석하지 않는다.",
        "개체와배열": {},
        "ID보존": {},
        "월드메모리와객체": {},
        "임무전이": [],
        "제약": ["시계열은 약 1초 간격 표본이다. 표본 사이 일시적인 변경은 별도 기능 검사로 확인해야 한다.",
                 "ID 해시는 정렬된 목록의 32비트 FNV-1a 보조 지표이며 암호학적 동일성 증명이 아니다.",
                 "운송/실내 상태 검사와 실제 렌더 검사는 사람의 직접 입력 확인을 대신하지 않는다.",
                 "소수 편성 단일 시드 실행이며 지속적인 최대 부하나 대규모 성능을 보장하지 않는다."]}
    for key in ["유닛", "작전", "이벤트", "승객", "포드", "사망", "점령", "파괴", "발사인원", "진입인원",
                "진입후회수", "경로노드합", "실내이동거리합", "완료공격ID집합"]:
        result["개체와배열"][key] = limits(rows, key)
    for key in ["중복유닛ID", "중복개인ID", "중복포드ID", "초기유닛누락", "초기개인누락"]:
        result["ID보존"][key] = limits(rows, key)
    for key in ["유닛ID해시", "개인ID해시", "포드ID해시", "개인포드연결해시"]:
        values = [row[key] for row in rows]
        result["ID보존"][key] = {"시작": values[0], "끝": values[-1], "서로다른값": len(set(values))}
    result["ID보존"]["새유닛발생수"] = int(frame["누적유닛ID"] - frame["초기유닛"])
    result["ID보존"]["새개인발생수"] = int(frame["누적개인ID"] - frame["초기개인"])
    result["ID보존"]["전체포드ID수"] = int(frame["누적포드ID"])
    result["ID보존"]["발사인원당포드1기"] = int(frame["누적포드ID"]) == int(rows[-1]["발사인원"])
    result["ID보존"]["유닛개인보존검사"] = (
        all(float(row[key]) == 0 for row in rows
            for key in ["중복유닛ID", "중복개인ID", "중복포드ID", "초기유닛누락", "초기개인누락"])
        and result["ID보존"]["새유닛발생수"] == 0
        and result["ID보존"]["새개인발생수"] == 0)
    for key in ["UObject추정개수", "프로세스물리MB", "진입기록", "기체피격기록", "요격기록", "대함피격기록",
                "기체레코드", "비행기체", "기체탄", "대함탄", "포드", "승객"]:
        result["월드메모리와객체"][key] = limits(world, key)
    profile_path = folder / "프로파일요약.json"
    if profile_path.exists():
        profile = load_json(profile_path).get("구간별프로파일", {}).get("사례0_단계3", {})
        result["실제측정구간CPU와GPUms"] = {label: profile[key]
            for label, key in [("게임스레드", "GameThreadTime"), ("렌더스레드", "RenderThreadTime"), ("GPU", "GPUTime")]
            if key in profile}
    # 프로세스 출범/초기 로딩/종료의 비용을 정상 플레이 비용으로 섞지 않는다.
    os_rows = [row for row in process.get("프로세스샘플", []) if 30 <= row["벽시계초"] <= 580]
    result["프로세스부하표본"] = {"구간설명": "프로세스 벽시계 30~580초; 엔진 프레임 구간과 동일한 경계는 아님", "지표": {}}
    for key in ["전체코어대비CPU백분율", "프로세스GPU3D최대백분율", "개인메모리MB", "핸들수"]:
        values = sorted(row[key] for row in os_rows if row.get(key) is not None)
        if values:
            result["프로세스부하표본"]["지표"][key] = {
                "표본": len(values), "평균": statistics.mean(values),
                "p95": values[math.ceil(len(values)*.95)-1], "최대": values[-1]}
    phases = ["함대 교전", "쉴드 비활성 창", "운송", "선내 작전", "철수", "완료"]
    outcomes = ["진행 중", "승리", "패배", "철수", "미결정"]
    previous = None
    for row in rows:
        state = (int(row["임무단계"]), int(row["임무결과"]))
        if state != previous:
            result["임무전이"].append({"벽시계초": float(row["벽시계초"]), "게임초": float(row["게임초"]),
                                    "단계": phases[state[0]], "결과": outcomes[state[1]]})
            previous = state
    completed = [row for row in rows if int(row["임무결과"]) != 0]
    if completed:
        result["완료후계속실행"] = {
            "완료관측벽시계초": float(completed[0]["벽시계초"]),
            "추가실제렌더초": float(rows[-1]["벽시계초"]) - float(completed[0]["벽시계초"]),
            "추가게임초": float(rows[-1]["게임초"]) - float(completed[0]["게임초"]),
            "작전기록수": limits(completed, "작전"), "이벤트수": limits(completed, "이벤트"),
            "완료공격ID집합": limits(completed, "완료공격ID집합"),
            "완료후ID해시종류수": {key: len({row[key] for row in completed})
                              for key in ["유닛ID해시", "개인ID해시", "포드ID해시", "개인포드연결해시"]}}
        after_world = [row for row in world if float(row["벽시계초"]) >= float(completed[0]["벽시계초"])]
        result["완료후계속실행"]["메모리와객체"] = {
            key: limits(after_world, key) for key in ["UObject추정개수", "프로세스물리MB"]}
    elapsed = float(rows[-1]["벽시계초"]) - float(rows[0]["벽시계초"])
    ids = result["개체와배열"]["완료공격ID집합"]
    result["중복방지집합누적"] = {
        "시작": ids["시작"], "끝": ids["끝"], "증가": ids["변화"],
        "초당증가": ids["변화"] / elapsed if elapsed else 0,
        "판정": "완료 공격 ID는 장면 초기화까지 보존하는 기존 구현이다. 이 집합에는 크기 상한이 없어 장시간 신규 공격이 지속되면 증가한다.",
        "동작변경": "없음. 중복 피해 방지를 유지하고 읽기 전용 접근자로 수량만 측정함."}
    if ids["변화"] > 0:
        result["제약"].append("완료 공격 ID 집합의 누적을 실측했다. 현재 메모리 변화가 작더라도 무제한 실행에 대한 메모리 상한 보장은 아니다.")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"통합 임무 수명 요약 저장: {output}")
    print(f"ID 보존={result['ID보존']['유닛개인보존검사']}, 완료 공격 ID={int(ids['시작'])}→{int(ids['끝'])}")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("folder", type=Path, help="완료된 OVPerfMission 진단 결과 폴더")
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[3] / "docs/validation/Phase3CD-Lifetime.json")
    args = parser.parse_args()
    summarize(args.folder.resolve(), args.output.resolve())
