"""Reproducible held-out experiments. Standard library only. No UE dependencies."""
import argparse,concurrent.futures,hashlib,json,math,statistics,time
from pathlib import Path
from engine import FleetBattle,HeroBattle,HybridBattle,P
ROOT=Path(__file__).parent
EVALUATION_SEEDS={'fleet':1000,'hero':4000,'hybrid':4000}
BALANCED=['melee','ranged','support','commander']
SCENARIOS={
 'F01':{'domain':'fleet','name':'호위함 동형 1:1','left':['frigate'],'right':['frigate']},
 'F02':{'domain':'fleet','name':'호위함5 대 바스티온1','left':['frigate']*5,'right':['bastion']},
 'F03':{'domain':'fleet','name':'백터2 대 바스티온1','left':['vector']*2,'right':['bastion']},
 'F04':{'domain':'fleet','name':'순양함5 대 전함1','left':['cruiser']*5,'right':['battleship']},
 'F05':{'domain':'fleet','name':'전함3 대 구축전함1','left':['battleship']*3,'right':['dreadnought']},
 'F06':{'domain':'fleet','name':'전함5 대 구축전함1','left':['battleship']*5,'right':['dreadnought']},
 'F07':{'domain':'fleet','name':'미사일 순양함3 대 일반 순양함3','left':['cruiser']*3,'right':['cruiser']*3,'missile_loadout':True,'motion':'fixed','distance':600},
 'F08':{'domain':'fleet','name':'미사일 순양함3 대 순양함2+방공함1','left':['cruiser']*3,'right':['cruiser']*2+['air_defense'],'missile_loadout':True,'motion':'fixed','distance':600},
 'F09':{'domain':'fleet','name':'자체 레이더 상실 순양함2 대 순양함2','left':['cruiser']*2,'right':['cruiser']*2,'no_radar_left':True,'motion':'fixed','distance':900},
 'F10':{'domain':'fleet','name':'자체 레이더 상실 순양함2+레이더함 대 순양함2','left':['cruiser']*2+['radar'],'right':['cruiser']*2,'no_radar_left':True,'motion':'fixed','distance':900},
 'H01':{'domain':'hero','name':'혼합4 대 혼합4','left':BALANCED,'right':BALANCED},
 'H02':{'domain':'hero','name':'혼합4 대 근거리4','left':BALANCED,'right':['melee']*4},
 'H03':{'domain':'hero','name':'혼합4 대 원거리4','left':BALANCED,'right':['ranged']*4},
 'H04':{'domain':'hero','name':'근거리4 대 원거리4','left':['melee']*4,'right':['ranged']*4},
 'H05':{'domain':'hero','name':'혼합4 대 병사12+지휘1','left':BALANCED,'right':['soldier']*12+['commander']},
 'H06':{'domain':'hero','name':'혼합4 대 병사16+지휘1','left':BALANCED,'right':['soldier']*16+['commander']},
 'H07':{'domain':'hero','name':'근거리1 대 병사4','left':['melee'],'right':['soldier']*4},
 'M01':{'domain':'hybrid','name':'근접 함대전, 영웅 미운용','policy':'fleet_only','goal':'shield'},
 'M02':{'domain':'hybrid','name':'근접 함대전, 함내 지원 보직','policy':'officers','goal':'shield'},
 'M03':{'domain':'hybrid','name':'쉴드실 강습, 포격 지속','policy':'boarding_reckless','goal':'shield'},
 'M04':{'domain':'hybrid','name':'쉴드실 강습, 잔존HP50%에서 사격 보류','policy':'boarding_safe','goal':'shield'},
 'M05':{'domain':'hybrid','name':'무기실 강습, 잔존HP50%에서 사격 보류','policy':'boarding_safe','goal':'weapon'},
 'M06':{'domain':'hybrid','name':'함교 강습, 잔존HP50%에서 사격 보류','policy':'boarding_safe','goal':'bridge'},
 'M07':{'domain':'hybrid','name':'적 방공함 추가, 쉴드실 강습','policy':'boarding_safe','goal':'shield','escort':True},
 'M08':{'domain':'hybrid','name':'적 방공함 추가, 함교 강습','policy':'boarding_safe','goal':'bridge','escort':True},
}
def make(case,seed):
 s=SCENARIOS[case]
 if s['domain']=='hero':return HeroBattle(s['left'],s['right'],seed)
 if s['domain']=='hybrid':return HybridBattle(seed,s['policy'],s['goal'],s.get('escort',False))
 loadout=None
 if s.get('missile_loadout'):
  loadout={(0,i):[{'kind':'missile','tier':2,'mount':'top' if j%2==0 else 'bottom'} for j in range(5)] for i in range(3)}
 b=FleetBattle(s['left'],s['right'],seed,distance=s.get('distance',700),motion=s.get('motion','orbit'),loadout=loadout)
 if s.get('no_radar_left'):
  for ship in b.ships:
   if ship.side==0 and ship.kind!='radar':ship.disabled['radar']='test_disabled'
 return b
def trial(job):
 case,seed,step=job;domain=SCENARIOS[case]['domain'];limit=360 if domain=='fleet' else 180 if domain=='hero' else 300
 result=make(case,seed).run(limit=limit,dt=step)
 return {'case':case,'seed':seed,'dt':step,**result}
def wilson(k,n):
 z=1.96;p=k/n;denom=1+z*z/n;center=(p+z*z/(2*n))/denom;delta=z*math.sqrt(p*(1-p)/n+z*z/(4*n*n))/denom
 return [round(100*(center-delta),2),round(100*(center+delta),2)]
def aggregate(rows):
 out={}
 for case,spec in SCENARIOS.items():
  rr=[r for r in rows if r['case']==case]
  if not rr:continue
  n=len(rr);wins=sum(r['winner']==0 for r in rr);out[case]={'name':spec['name'],'domain':spec['domain'],'n':n,'left_wins':wins,'right_wins':sum(r['winner']==1 for r in rr),'mutual':sum(r['winner']==-1 for r in rr),'timeouts':sum(r['winner'] is None for r in rr),'left_win_percent':round(100*wins/n,2),'left_win_wilson95_percent':wilson(wins,n),'median_seconds':round(statistics.median(r['seconds'] for r in rr),2)}
  for key in ('pd_kills','shots','objective','heroes_extracted','heroes_dead','heroes_rescue','pods_entered','captured'):
   if key in rr[0]:out[case]['mean_'+key]=round(statistics.mean(r[key] for r in rr),3)
  if spec['domain']=='hybrid':out[case]['phases']={p:sum(r['phase']==p for r in rr) for p in sorted(set(r['phase'] for r in rr))}
 return out
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--workers',type=int,default=1);ap.add_argument('--quick',action='store_true');ap.add_argument('--output',default='results.json');ap.add_argument('--reuse-fleet');ap.add_argument('--reuse-profile');args=ap.parse_args();started=time.monotonic();jobs=[]
 for case,s in SCENARIOS.items():
  count=4 if args.quick else 80 if s['domain']=='hero' else 32
  jobs.extend((case,EVALUATION_SEEDS[s['domain']]+i,.1) for i in range(count))
 cached=[];reuse=None
 if args.reuse_fleet:
  if not args.reuse_profile:ap.error('--reuse-profile is required with --reuse-fleet')
  prior=json.loads(Path(args.reuse_fleet).read_text());prior_profile=json.loads(Path(args.reuse_profile).read_text())
  component=hashlib.sha256((ROOT/'engine.py').read_text().split('class HybridBattle:')[0].encode()).hexdigest()
  previous_component=prior.get('fleet_hero_component_sha256') or (prior.get('reuse_provenance') or {}).get('unchanged_fleet_hero_source_sha256')
  if prior['engine_sha256']!=hashlib.sha256((ROOT/'engine.py').read_bytes()).hexdigest() and previous_component!=component:ap.error('Fleet engine component changed: cache rejected')
  if prior['profile_sha256']!=hashlib.sha256(Path(args.reuse_profile).read_bytes()).hexdigest():ap.error('Prior profile hash mismatch')
  for key in ('global','ships','weapons','radar','tier_damage','officers'):
   if prior_profile[key]!=P[key]:ap.error('Fleet profile inputs changed: cache rejected')
  wanted=set(jobs);cached=[r for r in prior['trials'] if SCENARIOS[r['case']]['domain']=='fleet' and (r['case'],r['seed'],r['dt']) in wanted]
  cache_ids={(r['case'],r['seed'],r['dt']) for r in cached}
  if len(cache_ids)!=len(cached):ap.error('Duplicate cached trials')
  jobs=[j for j in jobs if j not in cache_ids];reuse={'fleet_trials':len(cached),'prior_results_sha256':hashlib.sha256(Path(args.reuse_fleet).read_bytes()).hexdigest(),'prior_profile_sha256':prior['profile_sha256'],'engine_sha256':prior['engine_sha256'],'validated_fleet_profile_keys':['global','ships','weapons','radar','tier_damage','officers'],'fleet_hero_component_sha256':component,'note':'Fleet engine component and fleet inputs unchanged; full engine hash can differ for Hybrid-only corrections.'}
 if args.workers==1:rows=list(map(trial,jobs))
 else:
  with concurrent.futures.ProcessPoolExecutor(max_workers=args.workers) as pool:
   rows=[]
   for r in pool.map(trial,jobs,chunksize=4):
    rows.append(r)
    if len(rows)%32==0:print(f'{len(rows)}/{len(jobs)} completed',flush=True)
 rows=cached+rows;rows.sort(key=lambda r:(r['case'],r['seed']))
 # Additional seeds, resolution study only; no parameter retuning after held-out evaluation.
 sensitivity=[]
 for case in ('F01','F03','H02','M06'):
  for seed in ((2000,2001,2002) if case.startswith('F') else (5000,5001,5002)):
   sensitivity.append({'case':case,'seed':seed,'coarse':trial((case,seed,.1)),'fine':trial((case,seed,.05))})
 result={'profile_sha256':hashlib.sha256((ROOT/'profile.json').read_bytes()).hexdigest(),'engine_sha256':hashlib.sha256((ROOT/'engine.py').read_bytes()).hexdigest(),'fleet_hero_component_sha256':hashlib.sha256((ROOT/'engine.py').read_text().split('class HybridBattle:')[0].encode()).hexdigest(),'runner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'seed_start_by_domain':EVALUATION_SEEDS,'reuse_provenance':reuse,'fleet_dt':.1,'hero_dt':.1,'hybrid_dt':.1,'elapsed_seconds':round(time.monotonic()-started,2),'scenarios':SCENARIOS,'summary':aggregate(rows),'resolution_checks':sensitivity,'trials':rows}
 (ROOT/args.output).write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
 for case,r in result['summary'].items():print(case,r['left_win_percent'],r['median_seconds'],r.get('mean_objective'),r.get('mean_heroes_extracted'),flush=True)
 print(f'Saved {len(rows)} trials and {len(sensitivity)*2} resolution runs to {args.output}',flush=True)
if __name__=='__main__':main()
