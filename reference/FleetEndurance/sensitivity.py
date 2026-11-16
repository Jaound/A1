import json,hashlib,concurrent.futures
from pathlib import Path
import model
R=Path(__file__).parent
def trial(job):
 budget,seed,dt=job;model.S['dt']=dt
 r=model.Battle(model.roster(budget,'mixed'),model.roster(budget,'mixed'),seed,policy='weapon').run()
 return dict(budget=budget,seed=seed,dt=dt,**r)
if __name__=='__main__':
 jobs=[(b,s,dt) for b in (100,200,300) for s in (80000,80001,80002) for dt in (1,.5)]
 with concurrent.futures.ProcessPoolExecutor(max_workers=2) as pool:rows=list(pool.map(trial,jobs))
 (R/'sensitivity.json').write_text(json.dumps({'profile_sha256':hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),'model_sha256':hashlib.sha256((R/'model.py').read_bytes()).hexdigest(),'note':'Timestep diagnostic, independent seeds, not calibration or convergence certification','trials':rows},ensure_ascii=False,indent=2)+'\n')
 print('Sensitivity complete:',len(rows))
