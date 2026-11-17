"""Validate completed studies and publish an auditable Korean calibration report."""
from pathlib import Path
import csv,hashlib,json,statistics

ROOT=Path(__file__).resolve().parents[2]
HERE=Path(__file__).resolve().parent
NAMES={"none":"환경 없음","asteroid":"운석","radiation":"방사능","flare":"플레어","blackhole":"블랙홀"}
CASES=["mixed_100","mixed_200","mixed_300","boarding_100","battlecarrier_200"]

def load(name):
    value=json.loads((HERE/name).read_text())
    assert value["status"]=="completed",(name,value["status"])
    return value
def med(values): return statistics.median(values)
def mean_metric(rows,key):return statistics.mean(r["metrics"].get(key,0) for r in rows)
def main():
    final=load("final_results.json")
    evaluation=load("evaluation_results.json")
    sensitivity=load("sensitivity_results.json")
    probes=load("boundary_results.json")
    policy=load("flare_policy_results.json")
    cover=load("flare_cover_results.json")
    persistence=load("asteroid_persistence_results.json")
    assert final["trials"]==140 and len(final["rows"])==140
    assert all(r["active_environment_count"]<=1 for r in final["rows"])
    assert len({(r["environment"],r["variant"],r["case"],r["seed"]) for r in final["rows"]})==140
    selected=json.loads((HERE/"selected_profiles.json").read_text())
    stats=[]
    for hazard in ("asteroid","radiation","flare","blackhole"):
        for case in CASES:
            group=[r for r in final["rows"] if r["environment"]==hazard and r["case"]==case]
            assert len(group)==4
            assert all(r["parameters"]==selected[hazard] for r in group)
            controls=[r for r in final["rows"] if r["environment"]=="none" and r["variant"]==("flare_plain_control" if hazard=="flare" else "control") and r["case"]==case]
            assert {r["seed"] for r in group}=={r["seed"] for r in controls}
            completed=[r for r in group if r["winner"] is not None or r["outcome"]=="mutual"]
            stats.append({"environment":hazard,"case":case,"n":len(group),
                          "control_observed_seconds":med([r["seconds"] for r in controls]),
                          "observed_seconds":med([r["seconds"] for r in group]),
                          "unresolved":sum(r["winner"] is None and r["outcome"]!="mutual" for r in group),
                          "completed_median_seconds":med([r["seconds"] for r in completed]) if completed else None,
                          "environment_hull_loss_pct":statistics.mean(sum(v for k,v in r["metrics"].items() if k.startswith("env_hull_damage_"))/r["initial_total_hull_hp"]*100 for r in group),
                          "environment_wrecks_mean":mean_metric(group,"environment_wrecks"),
                          "entered_heroes_mean":mean_metric(group,"entered_heroes"),
                          "returned_heroes_mean":mean_metric(group,"returned_heroes")})
    with (HERE/"final_summary.csv").open("w",encoding="utf-8-sig",newline="") as f:
        writer=csv.DictWriter(f,fieldnames=list(stats[0]),lineterminator="\n")
        writer.writeheader();writer.writerows(stats)
    lines=[
        "# 우주 환경 단독 전투 비교 — E1 v0.1",
        "",
        "2026-11-17 KST. D123/D124에 따른 Python 설계 모형이다. **한 전투에 하나의 공간 환경만 활성화**했다. UE 실행·실제 모델 광선/충돌·최종 게임 밸런스 검증은 아니다.",
        "",
        "[기준 수치](../02_balance/SPACE_ENVIRONMENT_BALANCE.md) · [원시 자료/재현](../../reference/SpaceEnvironmentCalibration/README.md) · [CSV](../../reference/SpaceEnvironmentCalibration/final_summary.csv)",
        "",
        "## 표본과 선택 절차",
        "",
        "- 초기 약/기준/강 입력 탐색28회(600초)와100/200/300코스트 혼성·영웅 강습100·항공순양함200의 본 비교를 분리했다.",
        "- 최종 E1 자료는140개 고유 조건/시드 조합이다. 환경별20회씩80회, 일반 대조40회, 플레어 엄폐 지형만 있는 대조20회다. 조건당4시드이며 다른 환경의 피해를 함께 켜지 않았다.",
        "- 운석·방사능·블랙홀은173100~173103, 최종 플레어와 자체 대조군은178100~178103이다. 각 환경과 그 대조군은 같은 시드·편성이다.",
        "- 처음120회 비교와 최종140회를 합산하지 않는다. 최종 자료는 앞선 비교 중 영향 없는80행과 최종 플레어/대조60행으로 구성된다. 원래120회의플레어R120/지형 대조40행은 비교 이력으로만 남긴다.",
        "- 처음 수정된120회 비교 자체는60행 신규+의존성 검증 재사용60행이다. 재사용 범위는일반대조/운석/방사능이며 기존기록과별도복제해표본을늘리지않는다. AST·입력 해시 근거는reuse_proof.json과코드 스냅샷eac4063에서 확인한다.",
        "- 수동/Hold·FTL은 별도 경계로 확인한다. 본 전투는환경 비교를 위해자동 퇴각을끄고최대1800게임초까지관측했다. 따라서실제 플레이어의전략적퇴각손실과동일하지않다.",
        "- 원형 v0.6에서항공순양함주포2문·격납고상한4/8과실제장착2/4를구분하고강습조4영웅+6병력으로맞췄다. 재산정전인중형수송선은편성하지않았다.",
        "",
        "## 최종 단독 환경 결과",
        "",
        "관측 종료 중앙값이다. **미결정이 있는 행의1800초를승부 완료 시간으로 해석하지 않는다.** 조건당4시드의서술통계이며전체승률확정이나난이도등가보장이아니다.",
        "",
        "| 환경 | 편성 | 대조 관측(분) | 환경 관측(분) | 미결정/4 | 환경 기인 HP손실² | 환경 완파 평균 |",
        "| --- | --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for s in stats:
        label=CASES.index(s["case"])
        name={"mixed_100":"혼성100","mixed_200":"혼성200","mixed_300":"혼성300","boarding_100":"강습100","battlecarrier_200":"항공순양함200"}[s["case"]]
        lines.append(f'| {NAMES[s["environment"]]} | {name} | {s["control_observed_seconds"]/60:.2f} | {s["observed_seconds"]/60:.2f} | {s["unresolved"]} | {s["environment_hull_loss_pct"]:.2f}% | {s["environment_wrecks_mean"]:.2f} |')
    lines += [
        "",
        "² 초기 양측 총선체HP 대비 환경 직접 피해/그로 인한 코어 완파 등을 포함한 유효HP 감소다. 내부 재난·전술 변화·후속 일반 공격 손실을 모두 환경 직접 피해로 분류하지 않는다. 관측 기간도 다르므로 이 비율을 순수환경DPS처럼비교하지않는다.",
        "",
        "## 추가 비교와 발견",
        "",
        "- 운석HP를1800/6000/18000으로바꾼지형제거12회에서기준1800은100코스트144.5초,300코스트34.25초에마지막운석이제거됐다(각2시드중앙값).6000은179.25/63초,18000은100코스트두사례가600초에미완료였다. 이자료는완전전투승률에합산하지않는다.",
        "- 방사능저항0/25/50%의단독민감도비교를별도로수행했다. 연구가격/선행은이번자료로확정하지않는다.",
        "- 플레어는120m엄폐물·상시엄폐정책에서미결정이많았다. 위험도기반대응18회(같은조건재검사10+새시드쌍대8)와반경60/80/100m교정12회를별도로수행했다. 새AI가모든편성에항상우월하다는결과는아니다.",
        "- 최종플레어는반경60m의일반엄폐물과위험도기반대응을기준으로새시드60회(일반대조20+지형대조20+플레어20)로확인했다. 피해/정지3초를낮춰교착을가린것이아니다.",
        "- 플레어시험의고정엄폐물은다른환경이아니다. 운석피해/파괴와플레어를동시에작동시킨것으로계산하지않는다.",
        "- 블랙홀은무동력상태에서도실제로이동하며중심부에닿아완파된다.20초FTL경계시험에서순양함100m시작은도약전완파,200m이상시작은이조건에서도약했다. 이20초를함급별새FTL시간으로채택하지않는다.",
        "",
        "### 방사능 저항 민감도",
        "",
        "| 저항 | 환경 기인 선체HP손실 | 환경 완파 평균 |",
        "| --- | ---: | ---: |",
    ]
    for resistance in (0.,.25,.5):
        key=f"radiation/resistance_{resistance}/mixed_100"
        value=sensitivity["summary"][key]
        lines.append(f'| {resistance:.0%} | {value["mean_environment_hull_fraction"]*100:.2f}% | {value["metrics_mean"].get("environment_wrecks",0):.2f} |')
    lines += [
        "",
        "각2시드·900초의별도비교다. 저항계수의25/50% 감소와총전투손실감소는동일한비율이아니다.",
        "",
        "## 수정한 모델 결함과 검증",
        "",
        "- 파동중양측무장이잠깐멈춘것을영구교착으로조기종료하지않도록수정했다.",
        "- 중력회피가현재접근속도와제동여유를고려하도록보완하고,고정엄폐뒤사선재확보를추가했다.",
        "- 일부초기항모와엄폐물의겹침을발견했다. 해당엄폐자료를최종표본에서제외하고겹침검사를추가했다. 잘못된자료파일은삭제하지않고invalid명칭과사유를남겼다.",
        "- 환경기인코어완파의원인과중복완파집계를고쳤다. 관제상실함재기도방사능에노출되며,모함과별개인기체/포드의파동통과를검사한다.",
        "- 환경행동검사26개와기존IntegratedCombat검사36개를수행한다. UE의85개검사를이번환경모형에서실행했다는의미가아니다.",
        "",
        "## 수치 사용 한계",
        "",
        "- 시야는모형의관측/공개정보를쓰지만최종UE센서/메시경로를인증하지않는다. 지도환경은양측에알려진시험배치다.",
        "- 함체표면·장갑구획·내부시설은임시논리형상이다. 실제에셋의면적/차폐·장착점·질량과다르다.",
        "- 함재기환경위치는개체ID별비행프록시지만대함공격·상호교전·PD일부는기존집계근사다. 포드정지/재개와환경노출을연결했지만구조위치/납치/전용인양전체를검증하지않았다.",
        "- 함재기자체쉴드75·회복상세,플레어무쉴드판정과선내전/생명유지/FTL상호작용은시험가정이다. 실제게임의전함급강화/쉴드개조를모두포함하지않았다.",
        "- 블랙홀은함체구형표면접촉판정과모형용추력을사용한다. 함재기/포드견인·발사체중력·인양은미검증이다.",
        "- 자동항로는국소회피이며완전한3D항법/함대충돌/엄폐점점유를해결하지않는다. 일시정지/배속은게임시간개념으로설계했으나UE실제입력·FPS검증을대체하지않는다.",
        "- 시간간격.25/.5/1초민감도는별도30회자료에보존한다. 같은시드의승패불변이나수치수렴을가정하지않는다.",
        "- 기지/물류/연구가격·전략자원보상·위험구역건설·인양과환경중첩은이번실험범위가아니다.",
        "",
        "이프로필은실제UE환경구현을시작할초기값이다. 특히플레어의교착과엄폐점점유,실제모델의부분노출과블랙홀추력은후속실측으로보정해야한다.",
    ]
    target=ROOT/"docs/04_analysis/SPACE_ENVIRONMENT_RESULTS.md"
    target.write_text("\n".join(lines)+"\n",encoding="utf-8")
    manifest={"status":"completed","final_unique_trials":140,"one_hazard_assertion":True,
              "other_studies":{"pilot":28,"sensitivity":30,"boundary_episodes":49,"terrain_removal":12,"policy_trials":18,"cover_calibration":12},
              "main_execution_snapshot":"eac4063","unaffected_reuse_snapshot":"1bd1691",
              "files":{name:hashlib.sha256((HERE/name).read_bytes()).hexdigest()
                       for name in ["environment_battle.py","run_environment_sweep.py","run_final_flare.py",
                                    "selected_profiles.json","implementation_profile.json","benchmark_profiles.json","evaluation_results.json",
                                    "final_flare_results.json","final_results.json","sensitivity_results.json",
                                    "boundary_results.json","asteroid_persistence_results.json",
                                    "flare_policy_results.json","flare_cover_results.json","reuse_proof.json","final_reuse_proof.json"]}}
    (HERE/"environment_manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"final_trials":140,"summary":stats},ensure_ascii=False,indent=2))
if __name__=="__main__":main()
