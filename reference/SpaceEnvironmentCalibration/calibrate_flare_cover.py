"""Tune only flare shelter radius; do not enable an asteroid hazard."""
import hashlib,json,time
from concurrent.futures import ProcessPoolExecutor,as_completed
from pathlib import Path
from run_environment_sweep import trial,summarize
HERE=Path(__file__).resolve().parent
def main():
    selected=json.loads((HERE/"selected_profiles.json").read_text())
    tasks=[dict(stage="flare_cover_calibration",case=case,seed=seed,hazard="flare",
                variant=f"radius_{radius}",parameters={**selected["flare"],"cover_radius":radius,"cover_policy":"risk_based"},duration=1800)
           for radius in (60.,80.,100.) for case in ("mixed_100","mixed_300") for seed in (177100,177101)]
    rows=[];start=time.monotonic()
    with ProcessPoolExecutor(max_workers=4) as pool:
        for f in as_completed([pool.submit(trial,t) for t in tasks]):
            rows.append(f.result());print(f"cover radius {len(rows)}/{len(tasks)} {time.monotonic()-start:.1f}s",flush=True)
    out={"status":"completed","trials":len(rows),"scope":"calibration, not held-out evaluation","rows":rows,"summary":summarize(rows),
         "code_sha256":hashlib.sha256((HERE/"environment_battle.py").read_bytes()).hexdigest()}
    (HERE/"flare_cover_results.json").write_text(json.dumps(out,ensure_ascii=False,indent=2)+"\n")
    for k,v in out["summary"].items():print(k,v["median_observed_end_seconds"],v["unresolved"],flush=True)
if __name__=="__main__":main()
