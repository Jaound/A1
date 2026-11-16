"""Arithmetic/schema checks only. Does not import or execute any combat simulator."""
import json,math
from pathlib import Path
P=json.loads((Path(__file__).parent/'v05.json').read_text());r=P['repair'];h=P['heroes']
assert h['capacity_scope']=='faction_global' and h['max_active']==15
assert h['capacity_tiers']==[3,6,10,15] and h['command_cost_each']==1
assert h['roster_max_owned']>=h['max_active']
assert len(h['skill_point_levels'])==6 and len(h['skill_unlock_levels'])==4
assert (h['skill_max_rank']-1)*len(h['skill_unlock_levels'])>len(h['skill_point_levels'])
assert sum(q*v for q,v in zip(r['standard_material_load'],r['resource_volume']))<=r['material_cargo_volume']
assert math.isclose(sum(x['base_cost_fraction'] for x in r['jobs'].values()),.55)
assert all(u['requires_core']==u['tier'] for u in h['command_upgrades']) and h['command_upgrades'][0]['requires_academy']==0
for k,ship in P['hulls'].items():
 H=ship['hp'];base=max(r['base_min_hp_per_second'],H*r['base_fraction_hp_per_second']);field=base*r['field_speed_multiplier']
 assert field==base*.5
 for fraction in (.1,.25,.5):
  base_cost=[c*.3*fraction for c in ship['build_cost']];field_cost=[c*r['field_cost_multiplier'] for c in base_cost]
  assert all(math.isclose(f,b*1.5) for f,b in zip(field_cost,base_cost))
  assert math.isclose(H*fraction/field,2*(H*fraction/base))
assert math.isclose(max(64,.001*P['hulls']['corvette']['hp'])*.5,32)
assert math.isclose(.1*P['hulls']['dreadnought']['hp']/(max(64,.001*P['hulls']['dreadnought']['hp'])*.5),200)
assert sum(120+40*(l-1)+10*(l-1)**2 for l in range(1,20))==30210
for skill in h['skills'].values():
 assert skill['unlock'] in h['skill_unlock_levels']
 for value in skill.values():
  if isinstance(value,list):assert len(value)==3
print('PASS: design arithmetic only — faction15, skill budgets, cargo, 16 hull repair ratios and XP tables. No combat simulation run.')
