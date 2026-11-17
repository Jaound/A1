"""Focused single-hazard boundary episodes; distinct from the fleet-battle sample."""
from pathlib import Path
import hashlib
import json
from environment_battle import EnvironmentBattle, DEFAULTS

HERE=Path(__file__).resolve().parent

def main():
    selected=json.loads((HERE/"selected_profiles.json").read_text())
    rows=[]
    for distance in (0,200,425,650,825,900):
        for resistance in (0,.25,.5):
            b=EnvironmentBattle(["cruiser"],["cruiser"],175100,hazard="radiation",
                                parameters=selected["radiation"],radiation_resistance=resistance,heroes=False)
            for s in b.ships:s.pos=[distance,0,0]
            shield=sum(s.shield for s in b.ships)
            b.pre_environment(1);b.environment(1)
            rows.append({"kind":"radiation_probe","distance":distance,"resistance":resistance,
                         "one_environment":True,"shield_damage":shield-sum(s.shield for s in b.ships),
                         "damaged_cells":b.metric["env_radiation_exposed_cell_seconds"]})
    for cover_radius in (0,8,120):
        for shielded in (False,True):
            b=EnvironmentBattle(["cruiser"],["cruiser"],175100,hazard="flare",
                                parameters=selected["flare"],heroes=False)
            for s in b.ships:s.pos=[0,0,0]
            b.bodies=[] if not cover_radius else [{"id":0,"pos":[-200,0,0],"radius":cover_radius,"destroyed":False,"hp":float("inf")}]
            s=b.ships[0]
            if not shielded:s.active=False;s.shield=0
            exposure,total=b.exposed_samples(s)
            before=s.shield
            b.t=b.params["warning"]+1600/b.params["speed"]
            b.pre_environment(.5)
            rows.append({"kind":"flare_cover_probe","cover_radius":cover_radius,"shielded":shielded,
                         "exposure":len(exposure)/total,"shield_damage":before-s.shield,
                         "stop_seconds":max(0,s.emp_until-b.t),"one_environment":True})
    for hull in ("corvette","cruiser","battleship","dreadnought"):
        for distance in (100,200,400,600,900):
            b=EnvironmentBattle([hull],[hull],175100,hazard="blackhole",
                                parameters=selected["blackhole"],heroes=False)
            s=b.ships[0];s.pos=[distance,160,0]
            s.facilities["engine"]["destroyed"]=True
            for _ in range(2400):
                b.update_motion(s,.25);b.t+=.25
                if s.dead:break
            rows.append({"kind":"blackhole_engine_dead","hull":hull,"distance":distance,
                         "wrecked":s.dead,"contact_seconds":round(b.t,2) if s.dead else None,
                         "initially_in_contact":distance<=b.params["core_radius"]+s.d["radius"],
                         "one_environment":True})
    for distance in (100,200,400,600,900):
        b=EnvironmentBattle(["cruiser"],["cruiser"],175100,hazard="blackhole",
                            parameters=selected["blackhole"],heroes=False)
        s=b.ships[0];s.pos=[distance,160,0];s.ftl=20
        for own in b.ships:own.guns=[]
        for _ in range(42):b.tick(.5)
        rows.append({"kind":"blackhole_ftl","distance":distance,"charge_seconds":20,
                     "wrecked":s.dead,"escaped":s.escaped,"one_environment":True})
    result={"status":"completed","episodes":len(rows),"rows":rows,
            "scope":"Deterministic probes, not added to fleet win-rate samples",
            "one_environment_per_episode":True,
            "environment_code_sha256":hashlib.sha256((HERE/"environment_battle.py").read_bytes()).hexdigest(),
            "selected_profiles_sha256":hashlib.sha256((HERE/"selected_profiles.json").read_bytes()).hexdigest()}
    (HERE/"boundary_results.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__=="__main__":main()
