#!/usr/bin/env python3
"""Economy/queue feasibility model, not a UE combat or 3D construction simulator."""
from __future__ import annotations
import argparse,csv,json,math,random,statistics
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
# Costs/durations from docs/BASE_BALANCE_DRAFT.md, in game minutes.
ASSETS={
 'miner':([300,0,0],1.5,1,0), 'corvette':([450,0,0],2,1,1),
 'frigate':([600,0,0],2.5,1,1), 'destroyer':([1800,200,0],4,2,5),
 'cruiser':([2400,350,0],5,2,5), 'battleship':([7200,1200,100],8,4,25),
 'dreadnought':([28800,6000,800],16,8,125),
}
MODULES={
 'warehouse':([300,0,0],1), 'lab':([450,0,0],1.5),
 'yard_s':([650,0,0],2), 'yard_m':([1800,300,0],4),
 'yard_l':([6000,1200,80],7), 'yard_x':([18000,4000,600],12),
 'defense':([400,0,0],1.5), 'radar':([300,50,0],1),
 'academy':([700,100,0],2.5), 'personnel':([350,0,0],1.25),
 'logistics':([600,100,0],2), 'dock_l':([2600,500,40],4),
}
CORE={2:([2000,150,0],5),3:([9000,1500,0],10),4:([28000,6000,800],16)}
LAB={2:([800,150,0],2),3:([2400,500,50],4),4:([7000,1800,200],7)}
# Required core, lab grade, dependencies, costs, duration.
TECH={
 'eng2':(1,1,(),[800,100,0],4),
 'hull2':(2,2,('eng2',),[1200,200,0],5),
 'eng3':(2,2,('eng2',),[2500,400,0],6),
 'hull3':(3,3,('hull2',),[4000,700,50],8),
 'eng4':(3,3,('eng3',),[8000,1800,200],10),
 'hull4':(4,4,('hull3',),[10000,2200,300],12),
 'cannon2':(2,2,(),[1000,180,0],4),
 'cannon3':(3,3,('cannon2',),[3000,600,40],7),
 'cannon4':(4,4,('cannon3',),[9000,1800,250],10),
}
# Authored mission workloads: resource-site unlocks, force proxy, travel/combat/objective.
# Force points use the design's 1:5 class target. They DO NOT predict battle outcomes.
MISSIONS=[
 {'id':1,'name':'secure_nearby_route','force':3,'duration':[4,4,2]},
 {'id':2,'name':'capture_resource_sector','force':10,'duration':[6,6,4]},
 {'id':3,'name':'secure_rare_supply','force':30,'duration':[7,7,4]},
 {'id':4,'name':'defend_supply_network','force':60,'duration':[6,8,6]},
 {'id':5,'name':'break_outer_fortress','force':140,'duration':[8,10,6]},
 {'id':6,'name':'attack_enemy_headquarters','force':220,'duration':[10,12,8]},
]
TEN_MISSIONS=[
 {'id':i+1,'name':name,'force':force,'duration':duration}
 for i,(name,force,duration) in enumerate([
  ('tutorial_patrol',3,[4,4,2]),('secure_first_supply',8,[6,6,4]),
  ('raid_pirate_depot',15,[6,8,6]),('capture_rare_route',30,[7,8,7]),
  ('defend_outpost',50,[6,10,6]),('reopen_supply_network',75,[8,10,8]),
  ('attack_border_base',110,[8,12,8]),('siege_outer_fortress',150,[10,12,8]),
  ('counterattack',190,[10,14,10]),('enemy_headquarters',220,[10,16,10])])
]
CASES={
 'light':{'loss':.05,'damage':.10,'income':1.0},
 'standard':{'loss':.12,'damage':.20,'income':.85},
 'heavy':{'loss':.25,'damage':.35,'income':.65},
}

def vecadd(a,b):return [x+y for x,y in zip(a,b)]
def scaled(v,k):return [x*k for x in v]

class Sim:
 def __init__(self,profile='mixed',case='standard',seed=0,trace=False,policy='balanced',plan='six'):
  self.plan=plan;self.missions=MISSIONS if plan=='six' else TEN_MISSIONS;self.unlocks=(1,2,4) if plan=='six' else (2,4,6)
  self.policy=policy;self.reserved=[0.,0.,0.];self.rng=random.Random(seed);self.profile=profile;self.case=CASES[case];self.case_name=case;self.seed=seed
  self.real=0.;self.game=0.;self.cash=[1200.,0.,0.];self.income_total=[0.,0.,0.];self.spent=[0.,0.,0.];self.overflow=[0.,0.,0.]
  self.core=1;self.warehouses=1;self.modules=4;self.connections=4;self.builders=[]
  self.labs=[{'level':1,'job':None,'upgrading':False}];self.tech=set();self.researching=set()
  self.yards=[{'kind':'yard_s','jobs':[]}];self.installed={'warehouse':1,'lab':1,'yard_s':1}
  self.miners=[2,0,0];self.miner_jobs=[0,0,0];self.fleet={'corvette':1};self.built={'miner':2,'corvette':1}
  self.destroyed={};self.mission=None;self.completed=0;self.repair_due=[0.,0.,0.]
  self.milestones={};self.events=[];self.trace=[];self.keep_trace=trace;self.next_trace=0.;self.completion=None
  self.wait_funding=0.;self.wait_repair=0.;self.peak_modules=4
 def log(self,event,**extra):
  self.events.append({'real_min':round(self.real,3),'game_min':round(self.game,3),'event':event,**extra})
 def mark(self,name):
  if name not in self.milestones:self.milestones[name]={'real':round(self.real,3),'game':round(self.game,3)};self.log(name)
 def cap(self):return [6000+6000*self.warehouses,1000+1500*self.warehouses,100+300*self.warehouses]
 def afford(self,cost):return all(a+1e-7>=b for a,b in zip(self.cash,cost))
 def pay(self,cost):
  assert self.afford(cost);self.cash=[max(0.,a-b) for a,b in zip(self.cash,cost)];self.spent=vecadd(self.spent,cost)
 def power(self):return sum(ASSETS[k][3]*n for k,n in self.fleet.items())
 def active_combat(self):return self.mission is not None and self.mission['phase']==1
 def speed(self):
  # 20% of real time paused in a reproducible cycle; battle workload always 1x.
  sec=self.real*60
  if sec%60<12:return 0
  if self.active_combat():return 1
  if self.profile=='patient':return 1
  if self.profile=='fast':return 3
  return 3 if int(self.real)%4==0 else 2
 def pending_module(self,name):return any(j['kind']=='module' and j['name']==name for j in self.builders)
 def count(self,name):return self.installed.get(name,0)+sum(j['kind']=='module' and j['name']==name for j in self.builders)
 def construct(self,name):
  if len(self.builders)>=self.core or self.modules>=[0,8,16,28,44][self.core]:return False
  if self.connections>=[0,24,48,84,132][self.core]:return False
  cost,duration=MODULES[name];cost=vecadd(cost,[40,0,0])
  if not self.afford(cost):return False
  self.pay(cost);self.modules+=1;self.connections+=1
  self.builders.append({'kind':'module','name':name,'left':duration+.25});self.peak_modules=max(self.peak_modules,self.modules)
  return True
 def upgrade_core(self):
  level=self.core+1
  if level>4 or f'eng{level}' not in self.tech or any(j['kind']=='core' for j in self.builders) or len(self.builders)>=self.core:return False
  cost,duration=CORE[level]
  if not self.afford(cost):return False
  self.pay(cost);self.builders.append({'kind':'core','name':level,'left':duration});return True
 def upgrade_lab(self,index):
  lab=self.labs[index];level=lab['level']+1
  if level>self.core or lab['job'] or lab['upgrading'] or len(self.builders)>=self.core:return False
  cost,duration=LAB[level]
  if not self.afford(cost):return False
  self.pay(cost);lab['upgrading']=True;self.builders.append({'kind':'lab','name':(index,level),'left':duration});return True
 def tech_allowed(self,name):
  # Sample balanced player fields the current class before saving for the next core.
  if self.policy=='balanced' and name=='eng3' and self.built.get('destroyer',0)<1:return False
  if self.policy=='balanced' and name=='eng4' and self.built.get('battleship',0)<1:return False
  return True
 def start_research(self,index,name):
  lab=self.labs[index];core,level,deps,cost,duration=TECH[name]
  if not self.tech_allowed(name) or lab['job'] or lab['upgrading'] or name in self.tech|self.researching or self.core<core or lab['level']<level or any(x not in self.tech for x in deps) or not self.afford(cost):return False
  self.pay(cost);lab['job']={'name':name,'left':duration};self.researching.add(name);return True
 def start_ship(self,yard,name,ore=None):
  kind=yard['kind'];size=ASSETS[name][2]
  if kind=='yard_x':
   if size not in (4,8):return False
   need=size//4;cap=2
  else:
   cap={'yard_s':1,'yard_m':2,'yard_l':4}[kind];need=size
   if size>cap:return False
  if sum(j['need'] for j in yard['jobs'])+need>cap:return False
  cost,duration,*_=ASSETS[name]
  if not self.afford(cost):return False
  if self.policy=='balanced' and name!='miner' and any(a-b+1e-7<r for a,b,r in zip(self.cash,cost,self.reserved)):return False
  self.pay(cost);yard['jobs'].append({'name':name,'left':duration,'need':need,'ore':ore})
  if name=='miner':self.miner_jobs[ore]+=1
  return True
 def mining_caps(self):
  if self.completed>=self.unlocks[2]:return [30,30,30]
  if self.completed>=self.unlocks[1]:return [15,12,10]
  if self.completed>=self.unlocks[0]:return [8,6,0]
  return [4,2,0]
 def mining_targets(self):
  intended=([4,2,0] if self.core==1 else [8,6,0] if self.core==2 else [15,12,10] if self.core==3 else [30,30,30])
  return [min(a,b) for a,b in zip(intended,self.mining_caps())]
 def produce(self):
  target=self.mining_targets()
  desired='dreadnought' if 'hull4' in self.tech and self.installed.get('yard_x') else 'battleship' if 'hull3' in self.tech and self.installed.get('yard_l') else 'destroyer' if 'hull2' in self.tech and self.installed.get('yard_m') else 'frigate'
  # Small yards prioritize civilian expansion; larger yards prioritize current military class.
  for yard in self.yards:
   if yard['kind']=='yard_s':
    missing=[i for i in range(3) if self.miners[i]+self.miner_jobs[i]<target[i]]
    if missing:
     self.start_ship(yard,'miner',missing[0]);continue
   if self.power()+sum(ASSETS[j['name']][3] for y in self.yards for j in y['jobs'])<max(6,self.missions[min(self.completed,len(self.missions)-1)]['force']*1.2):
    if self.start_ship(yard,desired):continue
    if desired!='frigate' and yard['kind']=='yard_s':self.start_ship(yard,'frigate')
   # Use remaining standard yard space for civilian expansion.
   missing=[i for i in range(3) if self.miners[i]+self.miner_jobs[i]<target[i]]
   if missing and yard['kind']!='yard_x':self.start_ship(yard,'miner',missing[0])
 def strategy(self):
  # Explicit illustrative player policy; not an optimal planner or an AI design claim.
  if any(v>1e-7 for v in self.repair_due):
   if self.afford(self.repair_due):self.pay(self.repair_due);self.repair_due=[0.,0.,0.];self.log('repair_budget_paid')
   else:self.wait_repair+=self.tick_game;return
  # Keep enough warehouse capacity for the next core/capital investment.
  future=CORE[self.core+1][0] if self.core<4 else ASSETS['dreadnought'][0]
  if any(a<b for a,b in zip(self.cap(),future)) and not self.pending_module('warehouse'):self.construct('warehouse')
  self.upgrade_core()
  # Maintain one high-grade lab; a second independent lab after coreII.
  for i in range(len(self.labs)):self.upgrade_lab(i)
  if self.core>=2 and self.count('lab')<2:self.construct('lab')
  priorities=['eng2','hull2','eng3','hull3','eng4','hull4','cannon2','cannon3','cannon4']
  for i in range(len(self.labs)):
   for tech in priorities:
    if self.start_research(i,tech):break
  if self.core>=2 and 'hull2' in self.tech and not self.count('yard_m'):self.construct('yard_m')
  if self.core>=3 and 'hull3' in self.tech and not self.count('yard_l'):self.construct('yard_l')
  if self.core>=4 and 'hull4' in self.tech and not self.count('yard_x'):self.construct('yard_x')
  self.reserved=[0.,0.,0.]
  if self.policy=='balanced':
   candidates=[]
   if self.core<4 and f'eng{self.core+1}' in self.tech and not any(j['kind']=='core' for j in self.builders):candidates.append(CORE[self.core+1][0])
   for lab in self.labs:
    if lab['level']<self.core and not lab['job'] and not lab['upgrading']:candidates.append(LAB[lab['level']+1][0])
   for level,kind in ((2,'yard_m'),(3,'yard_l'),(4,'yard_x')):
    if f'hull{level}' in self.tech and not self.count(kind):candidates.append(vecadd(MODULES[kind][0],[40,0,0]))
   for name in priorities:
    core,lab,deps,cost,duration=TECH[name]
    if self.tech_allowed(name) and name not in self.tech|self.researching and self.core>=core and all(x in self.tech for x in deps) and any(l['level']>=lab and not l['job'] and not l['upgrading'] for l in self.labs):candidates.append(cost)
   for cost in candidates:
    if all(c==0 or self.miners[i]>0 for i,c in enumerate(cost)):
     self.reserved=list(cost);break
  self.produce()
  # Modest nonproductive support infrastructure, charged using documented module costs.
  targets={'defense':2 if self.core==1 else 4,'radar':1,'academy':int(self.core>=2),'personnel':int(self.core>=2),'logistics':int(self.core>=2),'dock_l':int(self.core>=3)}
  for name,n in targets.items():
   if self.count(name)<n:self.construct(name)
  if self.mission is None and self.completed<len(self.missions) and self.power()>=self.missions[self.completed]['force']:
   m=self.missions[self.completed];self.mission={'spec':m,'phase':0,'left':m['duration'][0],'fleet':dict(self.fleet)};self.log('mission_start',mission=m['id'],force=self.power())
 def finish_mission(self):
  m=self.mission['spec'];dispatched=self.mission['fleet'];self.completed+=1;self.log('mission_complete',mission=m['id'],dispatched_force=sum(ASSETS[k][3]*n for k,n in dispatched.items()),total_fleet_force=self.power())
  # Assumed attrition, not a combat simulation. Ships remaining stay in the fleet.
  survivors={}
  for name,n in list(dispatched.items()):
   lost=sum(self.rng.random()<self.case['loss'] for _ in range(n))
   survivors[name]=n-lost
   if lost:self.fleet[name]-=lost;self.destroyed[name]=self.destroyed.get(name,0)+lost
  # Aggregate repair bill based on surviving standard-package value, charged once.
  self.repair_due=[sum(ASSETS[k][0][r]*n for k,n in survivors.items())*.30*self.case['damage'] for r in range(3)]
  self.mission=None
  if self.completed==len(self.missions):self.completion={'real':round(self.real,3),'game':round(self.game,3)};self.mark('campaign_workload_complete')
 def tick(self,real_dt):
  speed=self.speed();dt=real_dt*speed;self.real+=real_dt;self.game+=dt;self.tick_game=dt
  if dt<=0:return
  if any(v>0 for v in self.repair_due):self.wait_funding+=dt
  rates=[self.miners[i]*[80,20,4][i]*self.case['income'] for i in range(3)]
  gain=scaled(rates,dt);self.income_total=vecadd(self.income_total,gain)
  caps=self.cap()
  for i in range(3):
   total=self.cash[i]+gain[i];self.overflow[i]+=max(0,total-caps[i]);self.cash[i]=min(caps[i],total)
  for job in list(self.builders):
   job['left']-=dt
   if job['left']>1e-8:continue
   self.builders.remove(job)
   if job['kind']=='core':self.core=job['name'];self.mark(f'core_{self.core}')
   elif job['kind']=='lab':i,level=job['name'];self.labs[i]['level']=level;self.labs[i]['upgrading']=False
   else:
    name=job['name'];self.installed[name]=self.installed.get(name,0)+1
    if name=='warehouse':self.warehouses+=1
    elif name=='lab':self.labs.append({'level':1,'job':None,'upgrading':False})
    elif name.startswith('yard_'):self.yards.append({'kind':name,'jobs':[]})
  for lab in self.labs:
   job=lab['job']
   if job:
    job['left']-=dt
    if job['left']<=1e-8:self.tech.add(job['name']);self.researching.remove(job['name']);self.mark('tech_'+job['name']);lab['job']=None
  for yard in self.yards:
   for job in list(yard['jobs']):
    job['left']-=dt
    if job['left']>1e-8:continue
    yard['jobs'].remove(job);name=job['name'];self.built[name]=self.built.get(name,0)+1
    if name=='miner':self.miners[job['ore']]+=1;self.miner_jobs[job['ore']]-=1
    else:self.fleet[name]=self.fleet.get(name,0)+1;self.mark('first_'+name)
  if self.mission:
   self.mission['left']-=dt
   if self.mission['left']<=1e-8:
    self.mission['phase']+=1
    if self.mission['phase']==3:self.finish_mission()
    else:self.mission['left']=self.mission['spec']['duration'][self.mission['phase']]
  self.strategy()
  assert min(self.cash)>-1e-6
  assert self.modules<=[0,8,16,28,44][self.core]
  for i in range(3):
   assert abs(1200*(i==0)+self.income_total[i]-self.spent[i]-self.overflow[i]-self.cash[i])<1e-4
  if self.keep_trace and self.real>=self.next_trace:
   self.record_trace()
   self.next_trace+=1
 def record_trace(self):
  self.trace.append({'real_min':round(self.real,3),'game_min':round(self.game,3),'core':self.core,'missions':self.completed,'power_proxy':self.power(),'general':round(self.cash[0],2),'advanced':round(self.cash[1],2),'rare':round(self.cash[2],2),'miner_g':self.miners[0],'miner_a':self.miners[1],'miner_r':self.miners[2],'modules_reserved':self.modules})
 def run(self,horizon=300,real_dt=.05):
  while self.real<horizon-1e-9 and self.completion is None:self.tick(real_dt)
  if self.keep_trace and (not self.trace or abs(self.trace[-1]['real_min']-self.real)>.0005):self.record_trace()
  return {'plan':self.plan,'policy':self.policy,'profile':self.profile,'case':self.case_name,'seed':self.seed,'completion':self.completion,'real_end':round(self.real,3),'game_end':round(self.game,3),'missions':self.completed,'core':self.core,'milestones':self.milestones,'cash':list(map(lambda x:round(x,2),self.cash)),'income':list(map(lambda x:round(x,2),self.income_total)),'spent':list(map(lambda x:round(x,2),self.spent)),'overflow':list(map(lambda x:round(x,2),self.overflow)),'built':self.built,'fleet':self.fleet,'losses':self.destroyed,'miners':self.miners,'modules_reserved':self.modules,'repair_due_at_end':self.repair_due,'repair_wait_game_min':round(self.wait_repair,3),'events':self.events}

def main():
 parser=argparse.ArgumentParser();parser.add_argument('--seeds',type=int,default=10);parser.add_argument('--plan',choices=['six','ten'],default='six');args=parser.parse_args()
 runs=[];representative=None
 for policy in ('balanced','fleet_first'):
  for profile in ('patient','mixed','fast'):
   for case in ('light','standard','heavy'):
    for seed in range(args.seeds):
     sim=Sim(profile,case,seed,trace=(policy=='balanced' and profile=='mixed' and case=='standard' and seed==0),policy=policy,plan=args.plan)
     result=sim.run();runs.append(result)
     if sim.keep_trace:
      representative=result
      with (OUT/f'trace_{args.plan}.csv').open('w',newline='') as f:
       writer=csv.DictWriter(f,fieldnames=list(sim.trace[0]),lineterminator='\n');writer.writeheader();writer.writerows(sim.trace)
 summaries=[]
 for policy in ('balanced','fleet_first'):
  for profile in ('patient','mixed','fast'):
   for case in ('light','standard','heavy'):
    group=[r for r in runs if r['policy']==policy and r['profile']==profile and r['case']==case];wins=[r['completion']['real'] for r in group if r['completion']]
    milestones={}
    for key in ('core_2','first_destroyer','core_3','first_battleship','core_4','first_dreadnought'):
     vals=[r['milestones'][key]['real'] for r in group if key in r['milestones']]
     milestones[key]={'count':len(vals),'median_real':round(statistics.median(vals),2) if vals else None}
    summaries.append({'policy':policy,'profile':profile,'case':case,'runs':len(group),'completed_by_300':len(wins),'median_completion_real':round(statistics.median(wins),2) if wins else None,'min_completion_real':round(min(wins),2) if wins else None,'max_completion_real':round(max(wins),2) if wins else None,'median_missions':statistics.median(r['missions'] for r in group),'milestones':milestones})
 output={'method':'Economy/queue/assumed mission workload model; no damage, hit, boarding, hero progression or 3D layout simulation','design_basis':'e221d23 BASE_BALANCE_DRAFT v0.1','seeds_per_cell':args.seeds,'tick_real_minutes':.05,'horizon_real_minutes':300,'case_assumptions':CASES,'mission_plan':args.plan,'missions':MISSIONS if args.plan=='six' else TEN_MISSIONS,'summaries':summaries,'representative':representative,'runs':[{k:v for k,v in r.items() if k!='events'} for r in runs]}
 header=json.dumps({k:v for k,v in output.items() if k!='runs'},ensure_ascii=False,indent=2)
 records=',\n'.join('    '+json.dumps(r,ensure_ascii=False,separators=(',',':')) for r in output['runs'])
 (OUT/f'results_{args.plan}.json').write_text(header[:-1].rstrip()+',\n  "runs": [\n'+records+'\n  ]\n}\n')
 print(json.dumps({'summaries':summaries,'representative':{k:v for k,v in representative.items() if k not in ('events',)}},ensure_ascii=False,indent=2))
if __name__=='__main__':main()
