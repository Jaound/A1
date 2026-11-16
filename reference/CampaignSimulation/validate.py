#!/usr/bin/env python3
"""Checks the model's inputs/accounting, not game play or combat correctness."""
from pathlib import Path
import json,re,runpy,math
HERE=Path(__file__).resolve().parent
namespace=runpy.run_path(str(HERE/'simulate.py'));Sim=namespace['Sim']
spec=(HERE.parents[1]/'docs/BASE_BALANCE_DRAFT.md').read_text()
def table(start,end):
 text=spec.split(start,1)[1].split(end,1)[0]
 return {cells[0].replace(' ',''):cells for line in text.splitlines() if line.startswith('|') and '---' not in line for cells in [[x.strip() for x in line.strip().strip('|').split('|')]]}
def duration(value):
 return sum(float(n)*(1 if unit=='분' else 1/60) for n,unit in re.findall(r'(\d+(?:\.\d+)?)(분|초)',value))
ships=table('| 표준 패키지 |','함재기20대는')
for name,label in {'miner':'채굴선','corvette':'초계함','frigate':'호위함','destroyer':'구축함','cruiser':'순양함','battleship':'전함','dreadnought':'구축전함'}.items():
 c=ships[label];cost,t,*_=namespace['ASSETS'][name]
 assert cost==list(map(int,c[1].split('/')))
 assert math.isclose(t,duration(c[2]))
modules=table('| 모듈 |','접안 점수는')
for name,label in {'warehouse':'자원창고','lab':'연구소I','yard_s':'소형조선','yard_m':'중형조선','yard_l':'대형조선','yard_x':'초대형조선','defense':'함포/빔방어','radar':'레이더','academy':'영웅아카데미','personnel':'인원지원소','logistics':'물류터미널','dock_l':'대형접안'}.items():
 c=modules[label];cost,t=namespace['MODULES'][name]
 assert cost==list(map(int,c[2].split('/')))
 assert math.isclose(t,duration(c[3]))
cores=table('| 코어 |','코어II/III/IV')
for level,key in ((2,'II'),(3,'III'),(4,'IV')):
 c=cores[key];cost,t=namespace['CORE'][level]
 assert cost==list(map(int,c[4].split('/'))) and math.isclose(t,duration(c[5]))
labs=table('| 연구소 |','등급은 기술 접근')
for level,key in ((2,'II'),(3,'III'),(4,'IV')):
 c=labs[key];cost,t=namespace['LAB'][level]
 assert cost==list(map(int,c[2].split('/'))) and math.isclose(t,duration(c[3]))
techs=table('| 연구 | 최소 코어/연구소 |','코어II 자체를')
for name,label in {'eng2':'기지공학II','eng3':'기지공학III','eng4':'기지공학IV','hull2':'중형함체','hull3':'대형함체','hull4':'초대형함체'}.items():
 c=techs[label];core,lab,deps,cost,t=namespace['TECH'][name]
 assert cost==list(map(int,c[3].split('/'))) and math.isclose(t,duration(c[4]))
tier_rows=table('| 계열별 티어 연구 |','함급 상한은')
for level in (2,3,4):
 c=tier_rows[f'T{level}'];*_,cost,t=namespace['TECH'][f'cannon{level}']
 assert cost==list(map(int,c[2].split('/'))) and math.isclose(t,duration(c[3]))
# Mixed capacity and XL exception.
s=Sim(policy='fleet_first');s.cash=[1e6]*3
y={'kind':'yard_l','jobs':[]}
assert s.start_ship(y,'destroyer') and s.start_ship(y,'miner',0) and s.start_ship(y,'miner',0)
assert not s.start_ship(y,'corvette')
y={'kind':'yard_x','jobs':[]}
assert not s.start_ship(y,'destroyer') and s.start_ship(y,'dreadnought') and not s.start_ship(y,'battleship')
y={'kind':'yard_x','jobs':[]}
assert s.start_ship(y,'battleship') and s.start_ship(y,'battleship') and not s.start_ship(y,'battleship')
# Ships built at home during a sortie must not teleport into the sortie's casualties.
s=Sim();s.case=dict(s.case,loss=1.0);s.fleet={'frigate':11};s.mission={'spec':namespace['MISSIONS'][0],'fleet':{'frigate':1}}
s.finish_mission();assert s.fleet['frigate']==10 and s.destroyed['frigate']==1 and sum(s.repair_due)==0
# Reproducibility and money conservation assertions run inside each tick.
a=Sim(seed=7).run(horizon=20);b=Sim(seed=7).run(horizon=20);assert a==b
# Published results: no negative resources, capacity bypass or regenerated combat losses.
for plan in ('six','ten'):
 data=json.loads((HERE/f'results_{plan}.json').read_text());assert data['seeds_per_cell']==10
 assert len(data['runs'])==180
 for r in data['runs']:
  assert r['modules_reserved']<=[0,8,16,28,44][r['core']]
  assert min(r['cash'])>=0
  assert r['completion'] is None or r['completion']['real']<=300
  for k,n in r['fleet'].items():assert n==r['built'].get(k,0)-r['losses'].get(k,0)
  for i in range(3):assert abs(1200*(i==0)+r['income'][i]-r['spent'][i]-r['overflow'][i]-r['cash'][i])<.04
 print(plan, '180 runs/accounting validated')
print('Canonical ship/module/core/lab/research cost & duration checks, mixed/XL capacity, sortie membership and deterministic replay passed.')
