"""Carry actual surviving passenger roles into indoor combat; no full fleet or room graph."""
import json,hashlib
from pathlib import Path
from study import pod_wave,heroes,HeroBattle
R=Path(__file__).parent
if __name__=='__main__':
 rows=[]
 for pd in (12,24):
  for d in (2,4,6):
   for seed in range(260000,260032):
    wave=pod_wave(heroes(4,'balanced')+['soldier']*6,pd,seed)
    arrived=[p['role'] for p in wave['pods'] if p['state']=='arrived']
    battle=HeroBattle(arrived,heroes(d,'balanced')+['soldier']*6,seed+1000).run() if arrived else {'winner':1,'seconds':0,'survivors':[0,d+6]}
    rows.append(dict(pd=pd,defender_heroes=d,seed=seed,attacker_roles=arrived,hero_rescue=wave['hero_rescue'],soldier_dead=wave['soldier_dead'],**battle))
 (R/'linked_results.json').write_text(json.dumps({'study_sha256':hashlib.sha256((R/'study.py').read_bytes()).hexdigest(),'profile_sha256':hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),'note':'4heroes+6soldiers, survivors regroup before combat, defenders6soldiers+2/4/6heroes. Actual pod IDs/roles preserved. No external shelling or return flight.','trials':rows},ensure_ascii=False,indent=2)+'\n');print('Linked:',len(rows))
