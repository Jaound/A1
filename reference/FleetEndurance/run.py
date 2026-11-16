import json,statistics,hashlib,argparse,concurrent.futures
from pathlib import Path
from collections import Counter
from model import Battle,roster,cost,P
R=Path(__file__).parent
def trial(job):
 budget,left,right,policy,seed,legacy,smart=job
 result=Battle(roster(budget,left),roster(budget,right),seed,policy,legacy,smart).run()
 return dict(budget=budget,left=left,right=right,policy=policy,seed=seed,legacy=legacy,smart=smart,**result)
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--n',type=int,default=24);ap.add_argument('--seed',type=int,default=10000);ap.add_argument('--output',default='results.json');ap.add_argument('--reuse-none');ap.add_argument('--reuse-profile');args=ap.parse_args();jobs=[]
 for budget in (100,200,300):
  cases=[('mixed','mixed','none',False,False),('mixed','heavy','none',False,False),('mixed','light','none',False,False),('mixed','missile','none',False,False),('mixed','carrier','none',False,False),('logistics','mixed','none',False,False),('no_pd','mixed','none',False,False),('mixed','mixed','weapon',False,False),('mixed','mixed','weapon',False,True),('mixed','mixed','capture',False,False),('mixed','mixed','pod',False,False),('mixed','mixed','none',True,False)]
  if budget>=200:cases += [('mixed','xl','none',False,False)]
  for l,r,policy,legacy,smart in cases:
   jobs += [(budget,l,r,policy,args.seed+(20000 if policy!='none' else 0)+i,legacy,smart) for i in range(args.n)]
 cached=[];reuse=None
 if args.reuse_none:
  prior=json.loads(Path(args.reuse_none).read_text());prior_p=json.loads(Path(args.reuse_profile).read_text())
  assert prior['model_sha256']==hashlib.sha256((R/'model.py').read_bytes()).hexdigest()
  assert prior['profile_sha256']==hashlib.sha256(Path(args.reuse_profile).read_bytes()).hexdigest()
  a=json.loads(json.dumps(prior_p));b=json.loads(json.dumps(P))
  for d in (a,b):
   for k in ('version','iteration_note'):d.pop(k,None)
   d['study'].pop('pod_hp',None)
  assert a==b,'Non-boarding data changed'
  cached=[r for r in prior['trials'] if r['policy']=='none'];ids={(r['budget'],r['left'],r['right'],r['policy'],r['seed'],r['legacy'],r['smart']) for r in cached};assert len(ids)==len(cached)
  jobs=[j for j in jobs if j not in ids];reuse={'count':len(cached),'prior_profile_sha256':prior['profile_sha256'],'prior_results_sha256':hashlib.sha256(Path(args.reuse_none).read_bytes()).hexdigest(),'proof':'Model unchanged; all parameters equal except pod_hp (unused when policy=none), version and note.'}
 with concurrent.futures.ProcessPoolExecutor(max_workers=2) as pool:
  rows=[]
  for row in pool.map(trial,jobs,chunksize=2):
   rows.append(row)
   if len(rows)%32==0:print(len(rows),len(jobs),flush=True)
 rows=cached+rows
 grouped={}
 for r in rows:
  key=f"{r['budget']}_{r['left']}_{r['right']}_{r['policy']}_{'old' if r['legacy'] else 'new'}_{'smart' if r['smart'] else 'screen'}";grouped.setdefault(key,[]).append(r)
 summary={}
 for key,rs in grouped.items():
  n=len(rs);summary[key]={'n':n,'wins':sum(r['winner']==0 for r in rs),'losses':sum(r['winner']==1 for r in rs),'timeouts':sum(r['winner'] is None for r in rs),'median_seconds':statistics.median(r['seconds'] for r in rs),'median_main_end':statistics.median(r['main_end'] for r in rs if r['main_end'] is not None) if any(r['main_end'] is not None for r in rs) else None,'median_first_loss':statistics.median(r['first_loss'] for r in rs if r['first_loss'] is not None),'objective_rate':statistics.mean(r.get('objective',0)>0 for r in rs),'mean_hero_dead':statistics.mean(r.get('hero_dead',0) for r in rs),'mean_heroes_home':statistics.mean(r.get('heroes_alive_end',0) for r in rs),'mean_entered':statistics.mean(r.get('heroes_entered',0) for r in rs),'mean_transit_rescue':statistics.mean(r.get('hero_rescue',0) for r in rs),'mean_launch':statistics.mean(r.get('launch_at',0) for r in rs),'mean_objective_time':statistics.mean(r.get('objective_at',0) for r in rs if r.get('objective')) if any(r.get('objective') for r in rs) else None}
 result={'profile_sha256':hashlib.sha256((R/'profile.json').read_bytes()).hexdigest(),'model_sha256':hashlib.sha256((R/'model.py').read_bytes()).hexdigest(),'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'indoor_source_sha256':hashlib.sha256((R.parent/'CombatSimulation/engine.py').read_bytes()).hexdigest(),'indoor_profile_sha256':hashlib.sha256((R.parent/'CombatSimulation/profile.json').read_bytes()).hexdigest(),'seed_start':args.seed,'boarding_seed_start':args.seed+20000,'reuse_provenance':reuse,'summary':summary,'rosters':{str(b):{k:dict(Counter(roster(b,k))) for k in ('mixed','heavy','light','missile','carrier','logistics','no_pd','xl') if roster(b,k) is not None} for b in (100,200,300)},'trials':rows}
 (R/args.output).write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 for k,v in summary.items():print(k,v,flush=True)
if __name__=='__main__':main()
