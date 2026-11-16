"""Separate sensitivity study, not retuning of the held-out combat profile."""
import hashlib,json,statistics,concurrent.futures
from pathlib import Path
import engine
ROOT=Path(__file__).parent

def trial(job):
 seed,policy=job
 original_damage=engine.Ship.damage;original_tick=engine.Ship.tick_shield;count=0
 def damage(s,amount,point,time):
  active=s.shield_active;recovery=s.recovery;loss=original_damage(s,amount,point,time)
  if policy=='break_only' and not active:s.recovery=recovery
  return loss
 def tick(s,dt,time):
  nonlocal count
  active=s.shield_active;original_tick(s,dt,time)
  if not active and s.shield_active:count+=1
 engine.Ship.damage=damage;engine.Ship.tick_shield=tick
 try:
  result=engine.FleetBattle(['frigate'],['frigate'],seed).run(dt=.1)
  return {'seed':seed,'policy':policy,'reactivations':count,**result}
 finally:engine.Ship.damage=original_damage;engine.Ship.tick_shield=original_tick

def main():
 jobs=[(seed,policy) for seed in range(3000,3008) for policy in ('every_hit','break_only')]
 with concurrent.futures.ProcessPoolExecutor(max_workers=2) as pool:rows=list(pool.map(trial,jobs))
 summary={}
 for policy in ('every_hit','break_only'):
  rr=[r for r in rows if r['policy']==policy];summary[policy]={'n':len(rr),'median_seconds':statistics.median(r['seconds'] for r in rr),'mean_reactivations':statistics.mean(r['reactivations'] for r in rr),'timeouts':sum(r['winner'] is None for r in rr)}
 out={'purpose':'Shield-delay refresh sensitivity only, not adopted policy','engine_sha256':hashlib.sha256((ROOT/'engine.py').read_bytes()).hexdigest(),'profile_sha256':hashlib.sha256((ROOT/'profile.json').read_bytes()).hexdigest(),'dt':.1,'scenario':'frigate1 vs frigate1 orbit, baseline settings','summary':summary,'trials':rows}
 (ROOT/'shield_delay_results.json').write_text(json.dumps(out,ensure_ascii=False,indent=2)+'\n');print(json.dumps(summary,indent=2),flush=True)
if __name__=='__main__':main()
