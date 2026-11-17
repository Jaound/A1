"""Final E1 flare validation on new seeds, with paired plain/shelter controls."""
from pathlib import Path
from concurrent.futures import ProcessPoolExecutor,as_completed
import hashlib,json,time
from run_environment_sweep import trial,summarize,hashes
HERE=Path(__file__).resolve().parent
def main():
    params=json.loads((HERE/"selected_profiles.json").read_text())
    historical=json.loads((HERE/"evaluation_results.json").read_text())
    tasks=[];cases=["mixed_100","mixed_200","mixed_300","boarding_100","battlecarrier_200"]
    for hazard,variant in (("none","flare_plain_control"),("none","flare_shelter_control"),("flare","selected")):
        for case in cases:
            for seed in range(178100,178104):
                fields={} if variant=="flare_plain_control" else {"static_shelter":True,"cover_radius":params["flare"]["cover_radius"]} if hazard=="none" else params["flare"]
                tasks.append(dict(stage="final_flare_validation",case=case,seed=seed,hazard=hazard,
                                  variant=variant,parameters=fields,duration=1800))
    rows=[];start=time.monotonic()
    with ProcessPoolExecutor(max_workers=4) as pool:
        for hazard in ("none","flare"):
            batch=[t for t in tasks if t["hazard"]==hazard]
            print(f"FINAL {hazard} {len(batch)} trials",flush=True)
            for future in as_completed([pool.submit(trial,t) for t in batch]):
                r=future.result();assert r["active_environment_count"]<=1
                r["source_study"]="final_flare_validation"
                rows.append(r)
                if len(rows)%4==0:print(f"final flare {len(rows)}/60 {time.monotonic()-start:.1f}s",flush=True)
    new={"status":"completed","rows":rows,"summary":summarize(rows),"trials":60,"hashes":hashes(),
         "selected_profile_sha256":hashlib.sha256((HERE/"selected_profiles.json").read_bytes()).hexdigest()}
    (HERE/"final_flare_results.json").write_text(json.dumps(new,ensure_ascii=False,indent=2)+"\n")
    retained=[]
    for r in historical["rows"]:
        if r["environment"] in ("asteroid","radiation","blackhole") or (r["environment"]=="none" and r["variant"]=="control"):
            r["source_study"]="main_evaluation"
            retained.append(r)
    assert len(retained)==80 and len(rows)==60
    merged=retained+rows
    assert len({(r["environment"],r["variant"],r["case"],r["seed"]) for r in merged})==140
    final={"status":"completed","trials":140,"rows":merged,"summary":summarize(merged),
           "current_profiles":params,"one_environment_per_battle":True,
           "main_results_sha256":hashlib.sha256((HERE/"evaluation_results.json").read_bytes()).hexdigest(),
           "final_flare_results_sha256":hashlib.sha256((HERE/"final_flare_results.json").read_bytes()).hexdigest(),
           "provenance":{"main_evaluation_rows":80,"final_flare_and_controls_rows":60,
                         "hazard_trials":80,"plain_control_trials":40,"shelter_control_trials":20,
                         "warning":"Never add the original 120-reference, pilot, policy rechecks and merged140 as independent repetitions."}}
    (HERE/"final_results.json").write_text(json.dumps(final,ensure_ascii=False,indent=2)+"\n")
    print("COMPLETE: final E1 contains 140 unique trials, only one hazard each.",flush=True)
if __name__=="__main__":main()
