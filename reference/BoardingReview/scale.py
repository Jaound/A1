import json,sys,hashlib,statistics,concurrent.futures
from pathlib import Path
R=Path(__file__).parent;sys.path.insert(0,str(R.parent/'FleetEndurance'))
from model import Battle,roster,cost,P

def run(job):
 case,budget,seed=job
 if case=='replicate100':rs=roster(100,'mixed')*(budget//100)
 else:rs=roster(budget,'mixed')
 if case=='replaceXL':
  i=rs.index('dreadnought');rs=rs[:i]+['battleship']*5+rs[i+1:]
 assert cost(rs)==budget
 b=Battle(rs,rs,seed);result=b.run()
 hp=sum(P['ships'][k]['hp'] for k in rs);shield=sum(P['ships'][k]['shield'] for k in rs)
 raw=sum(sum(P['weapons'][w['kind']]['damage']*P['tier_damage'][str(w['tier'])]*P['weapons'][w['kind']]['salvo']/P['weapons'][w['kind']]['period'] for w in P['ships'][k]['weapons'])+P['ships'][k].get('fighters',0)*20/1.2 for k in rs)
 return dict(case=case,budget=budget,seed=seed,ships=len(rs),hull=hp,shield=shield,nominal_dps=raw,**result)
if __name__=='__main__':
 jobs=[(case,b,250000+s) for case,b in [('original',100),('original',200),('original',300),('replicate100',200),('replicate100',300),('replaceXL',300)] for s in range(16)]
 with concurrent.futures.ProcessPoolExecutor(max_workers=2) as pool:rows=list(pool.map(run,jobs,chunksize=2))
 root=R.parent/'FleetEndurance';out={'source':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in ('profile.json','model.py')},'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'note':'Original v0.3c unchanged: isolate scale/composition; assault v0.4 candidate is not mixed into causal controls','trials':rows}
 (R/'scale_results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n')
 for case,b in [('original',100),('original',200),('original',300),('replicate100',200),('replicate100',300),('replaceXL',300)]:
  rr=[x for x in rows if x['case']==case and x['budget']==b];print(case,b,statistics.median(x['seconds'] for x in rr),rr[0]['hull'],rr[0]['nominal_dps'],flush=True)
