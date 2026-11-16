import json,hashlib,concurrent.futures
from pathlib import Path
from run import trial
R=Path(__file__).parent
if __name__=='__main__':
 jobs=[(b,'mixed','mixed','none',70000+i,False,False) for b in (100,200,300) for i in range(24)]
 with concurrent.futures.ProcessPoolExecutor(max_workers=2) as pool:rows=list(pool.map(trial,jobs,chunksize=2))
 (R/'paired_controls.json').write_text(json.dumps({'profile_sha256':hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),'model_sha256':hashlib.sha256((R/'model.py').read_bytes()).hexdigest(),'purpose':'Matched fleet-only controls for boarding seeds70000+; do not pool with primary10000+ baselines','trials':rows},ensure_ascii=False,indent=2)+'\n')
 print('Matched controls complete:',len(rows))
