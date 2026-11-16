"""Stress the unsupported interior geometry with 3x travel time; not a real layout simulation."""
import json,hashlib,concurrent.futures,statistics
from pathlib import Path
from model import Battle,roster
R=Path(__file__).parent
class Travel:
 def __init__(self,base,mult):self.base=base;self.mult=mult
 def uniform(self,a,b):return self.base.uniform(a,b)*self.mult
 def choice(self,values):return self.base.choice(values)
def trial(job):
 b,seed,mult=job;fight=Battle(roster(b,'mixed'),roster(b,'mixed'),seed,policy='weapon');fight.board_rng=Travel(fight.board_rng,mult)
 return dict(budget=b,seed=seed,travel_multiplier=mult,**fight.run())
if __name__=='__main__':
 jobs=[(b,120000+i,m) for b in (100,200,300) for i in range(6) for m in (1,3)]
 with concurrent.futures.ProcessPoolExecutor(max_workers=2) as pool:rows=list(pool.map(trial,jobs))
 (R/'route_checks.json').write_text(json.dumps({'profile_sha256':hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),'model_sha256':hashlib.sha256((R/'model.py').read_bytes()).hexdigest(),'override':'Interior/return walk uniform10–25s multiplied by1 or3; independent seeds120000+, not tuning','trials':rows},ensure_ascii=False,indent=2)+'\n');print('Route sensitivity complete:',len(rows))
