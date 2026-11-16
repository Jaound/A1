"""Separate exploratory calibration and held-out evaluation; JSON contains every trial."""
import argparse
import hashlib
import json
import statistics
import time
from collections import Counter,defaultdict
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

from model import Battle, P, C, roster
from indoor import Unit, indoor_step
from dependencies import validate_reuse

HERE=Path(__file__).parent


def indoor_trial(seed,attack=4,defense=4,level=10,enemy_level=None,gear=0,pattern='mixed',scale=1.):
    b=Battle(['dreadnought'],['dreadnought'],seed,heroes=False,layout_scale=scale)
    s=b.ships[1];s.units=[]
    kinds=['melee','ranged','support','commander'] if pattern=='mixed' else [pattern]*4
    for side,count,node,lv in ((0,attack,12,level),(1,defense,s.layout.bridge,enemy_level or level)):
        for i in range(count):b.add_unit(s,kinds[i%4],side,lv,gear,node=node)
        for i in range(8):b.add_unit(s,'soldier',side,node=node if side==0 else s.layout.bridge+(-1 if i>=4 else 0))
    initial={u.uid:(u.side,u.hero) for u in s.units};completed=False;work=0.;when=None
    while b.t<600:
        indoor_step(s,b.t,.25,b.board_rng,{0:(s.layout.bridge,False),1:(s.layout.bridge,False)},b.metric)
        alive=[u for u in s.units if u.alive()]
        if not any(u.side==0 for u in alive) or not any(u.side==1 for u in alive):break
        b.t+=.25
    attackers=[u for u in s.units if u.alive() and u.side==0]
    defenders=[u for u in s.units if u.alive() and u.side==1]
    winner=0 if attackers and not defenders else 1 if defenders and not attackers else None
    return {'seed':seed,'winner':winner,'seconds':b.t,'attack':attack,'defense':defense,'level':level,
            'enemy_level':enemy_level or level,'pattern':pattern,'gear':gear,'surviving_heroes':[sum(u.hero for u in attackers),sum(u.hero for u in defenders)],
            'metrics':dict(b.metric),'outcome':'timeout' if attackers and defenders else 'mutual' if not attackers and not defenders else 'decided'}


def run_case(task):
    case,n,seed=task;results=[]
    for i in range(n):
        if case['domain']=='indoor':r=indoor_trial(seed+i,**case['options'])
        else:
            b=Battle(roster(case['left_cost'],case['left']),roster(case['right_cost'],case['right']),seed+i,**case['options'])
            r=b.run(case.get('dt'));r['domain']=case['domain']
            r['loadout']={'left':dict(Counter(roster(case['left_cost'],case['left']))),'right':dict(Counter(roster(case['right_cost'],case['right'])))}
        r['case']=case['id'];results.append(r)
    return case,results


def cases(explore=False):
    out=[]
    def fleet(name,left_cost=100,right_cost=None,left='mixed',right='mixed',domain='fleet',n=8,**options):
        out.append({'id':name,'domain':domain,'left_cost':left_cost,'right_cost':right_cost or left_cost,'left':left,'right':right,'n':n,'options':options})
    if explore:
        for budget in (100,200,300):fleet(f'pilot_{budget}',budget,n=4,heroes=False)
        for policy in ('off','until_objective','until_exit'):fleet('pilot_board_'+policy,100,n=4,mode='pod',protection=policy,level=10)
        return out
    for budget in (100,200,300):
        fleet(f'fleet_mixed_{budget}',budget,n=16,heroes=False)
        for kind in ('heavy','missile','carrier','xl','logistics','no_pd'):
            fleet(f'fleet_{kind}_{budget}',budget,right=kind,heroes=False)
        fleet(f'fleet_light_{budget}',budget,right='light',n=4,heroes=False)
        for method in ('pod','assault'):
            for policy in ('off','until_objective','until_exit'):
                fleet(f'linked_{budget}_{method}_{policy}',budget,domain='linked',mode=method,protection=policy,level=10)
        fleet(f'heroes_only_{budget}',budget,domain='linked',mode='none',level=10)
    for attack in (1,2,4):
        for defense in (0,2,4,6):
            for level in (1,10,20):
                out.append({'id':f'indoor_{attack}_{defense}_L{level}','domain':'indoor','n':16,'options':{'attack':attack,'defense':defense,'level':level}})
    for pattern in ('melee','ranged','support','commander'):
        out.append({'id':'indoor_focus_'+pattern,'domain':'indoor','n':24,'options':{'pattern':pattern}})
    for lv in (5,15):out.append({'id':f'indoor_growth_L{lv}','domain':'indoor','n':24,'options':{'level':lv}})
    for tier in (1,4):out.append({'id':f'indoor_gear_T{tier}','domain':'indoor','n':24,'options':{'gear':tier}})
    for rank in (0,3):fleet(f'hangar_capacity_{rank}',100,left='carrier',right='carrier',capacity_rank=rank,heroes=False)
    for rank in (0,5):fleet(f'fighter_research_{rank}',100,left='carrier',right='carrier',fighter_rank=rank,heroes=False)
    fleet('fighter_refit',100,left='carrier',right='carrier',refit=True,heroes=False)
    for enemy in (75,125,150):fleet(f'mission_100_vs_{enemy}',100,enemy,domain='mission',level=10)
    fleet('weapon_aim',100,aim='weapons',level=10)
    fleet('baseline_hull_aim',100,aim='hull',level=10)
    fleet('linked_weapon_override',100,aim='weapons',force_weapons=True,mode='pod',level=10,domain='linked')
    fleet('bridge_capture',100,mode='assault',objective='capture',level=10,domain='linked')
    fleet('facility_sabotage',100,mode='assault',objective='sabotage',level=10,domain='linked')
    for lv in (1,20):fleet(f'linked_growth_L{lv}',100,mode='assault',level=lv,domain='linked')
    for lv in (5,20):out.append({'id':f'indoor_level_gap_{lv}','domain':'indoor','n':24,'options':{'level':10,'enemy_level':lv}})
    fleet('deep_layout',300,mode='pod',level=10,layout_scale=1.8,domain='sensitivity',n=4)
    # Same seed and scenario; numerical time-step/order checks are not tuning samples.
    for step in (.5,1.,2.):
        fleet(f'dt_{step}',100,heroes=False,domain='sensitivity',n=4)
        out[-1]['dt']=step
    fleet('reverse_order',100,heroes=False,reverse=True,domain='sensitivity',n=4)
    return out


def summary(rows):
    groups=defaultdict(list)
    for r in rows:groups[r['case']].append(r)
    out={}
    for key,rs in groups.items():
        times=[r['seconds'] for r in rs]
        out[key]={'n':len(rs),'left_wins':sum(r['winner']==0 for r in rs),'right_wins':sum(r['winner']==1 for r in rs),
                  'unresolved':sum(r['winner'] is None for r in rs),'median_seconds':statistics.median(times),
                  'outcomes':dict(Counter(r['outcome'] for r in rs))}
        for field in ('main_seconds','first_loss_seconds','target_survival_after_entry'):
            vals=[r[field] for r in rs if r.get(field) is not None]
            out[key]['median_'+field]=statistics.median(vals) if vals else None
        for metric in ('objectives','entered_heroes','returned_heroes','rescued_heroes','captured_heroes','hero_wreck_deaths','repair_hull_hp','hero_repair_hp','weapons_destroyed','fighter_losses','launch_to_entry','entry_to_objective','entry_to_exit'):
            out[key]['mean_'+metric]=statistics.mean(r.get('metrics',{}).get(metric,0) for r in rs)
        out[key]['objective_trials']=sum(r.get('metrics',{}).get('objectives',0)>0 for r in rs)
        out[key]['entry_trials']=sum(r.get('metrics',{}).get('entered_heroes',0)>0 for r in rs)
        out[key]['wreck_before_exit_trials']=sum(r.get('target_wreck_before_exit',False) for r in rs)
    return out


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--explore',action='store_true');parser.add_argument('--workers',type=int,default=2)
    parser.add_argument('--only');parser.add_argument('--n',type=int);parser.add_argument('--output');parser.add_argument('--reuse-indoor');args=parser.parse_args()
    spec=cases(args.explore)
    if args.only:spec=[c for c in spec if args.only in c['id']]
    if args.n:
        for c in spec:c['n']=args.n
    base=610000 if args.explore else 810000
    # Matched seeds across conditions permit paired comparisons; separate RNG for indoor decisions.
    reused=[];reuse_metadata=None
    if args.reuse_indoor:
        assert not args.explore and not args.n
        previous,reuse_metadata=validate_reuse(HERE,args.reuse_indoor)
        ids={c['id']:c for c in spec if c['domain']=='indoor'}
        old={c['id']:c for c in previous['cases']}
        assert all(old[key]==case for key,case in ids.items())
        reused=[{**row,'reused_indoor':True} for row in previous['trials'] if row['case'] in ids]
        reuse_metadata['rows']=len(reused);reuse_metadata['cases']=list(ids)
    tasks=[(case,case['n'],base) for case in spec if not (args.reuse_indoor and case['domain']=='indoor')]
    output=HERE/(args.output or ('calibration.json' if args.explore else 'results.json'))
    rows=list(reused);start=time.monotonic()
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        for i,(case,result) in enumerate(pool.map(run_case,tasks),1):
            rows+=result;print(f'{i}/{len(tasks)} {case["id"]}: {len(result)} trials, {time.monotonic()-start:.1f}s elapsed',flush=True)
    hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (HERE/'profile.json',HERE/'model.py',HERE/'indoor.py',HERE/'run.py',HERE/'dependencies.py')}
    payload={'version':P['version'],'exploratory':args.explore,'seed_base':base,'hashes':hashes,'reused_indoor':reuse_metadata,'cases':spec,'summary':summary(rows),'trials':rows}
    output.write_text(json.dumps(payload,ensure_ascii=False,indent=2)+'\n');print('WROTE',output,len(rows),'trials',flush=True)


if __name__=='__main__':main()
