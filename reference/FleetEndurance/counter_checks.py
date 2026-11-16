"""Additional predeclared counters: a mixed-vs-spam loss is not a complete meta ranking."""
import json,hashlib,concurrent.futures
from pathlib import Path
from model import Battle,roster,cost
R=Path(__file__).parent
def trial(job):
 b,kind,seed=job
 left=roster(b,'carrier' if kind=='carrier_vs_light' else 'mixed')
 if kind=='mixed_anti_screen':left[left.index('battleship')]='carrier'
 right=roster(b,'light');assert cost(left)==cost(right)==b
 return dict(budget=b,case=kind,seed=seed,**Battle(left,right,seed).run())
if __name__=='__main__':
 jobs=[(b,k,90000+i) for b in (100,200,300) for k in ('carrier_vs_light','mixed_anti_screen') for i in range(8)]
 with concurrent.futures.ProcessPoolExecutor(max_workers=2) as pool:rows=list(pool.map(trial,jobs,chunksize=2))
 (R/'counter_checks.json').write_text(json.dumps({'profile_sha256':hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),'model_sha256':hashlib.sha256((R/'model.py').read_bytes()).hexdigest(),'trials':rows},ensure_ascii=False,indent=2)+'\n')
 print('Counter checks complete:',len(rows))
