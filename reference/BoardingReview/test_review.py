import json,unittest,hashlib
from pathlib import Path
from study import CFG,pod_wave,assault_transit,heroes
R=Path(__file__).parent
class Review(unittest.TestCase):
 def test_ten_people_have_ten_pods_and_separate_clocks(self):
  wave=pod_wave(['soldier']*10,0,1);self.assertEqual(len(wave['pods']),10);self.assertEqual(len({x['id'] for x in wave['pods']}),10);self.assertEqual(wave['pods'][-1]['launch'],2.25)
 def test_original_passenger_roles_survive_manifest(self):
  people=heroes(4,'balanced')+['soldier']*6;wave=pod_wave(people,0,2);self.assertEqual([p['role'] for p in wave['pods']],people);self.assertEqual(wave['arrived'],10)
 def test_losses_are_local_and_heroes_rescuable(self):
  wave=pod_wave(heroes(4,'balanced')+['soldier']*6,100,3)
  for pod in wave['pods']:
   if pod['hp']<=0:self.assertEqual(pod['state'],'soldier_dead' if pod['role']=='soldier' else 'hero_rescue')
  self.assertEqual(wave['arrived']+wave['hero_rescue']+wave['soldier_dead'],10)
 def test_pd_only_cannot_damage_assault_ship(self):
  a=assault_transit(0,4,0);b=assault_transit(0,4,24);self.assertEqual(a['hp'],b['hp']);self.assertEqual(a['shield'],b['shield']);self.assertTrue(b['arrived'])
 def test_anti_ship_damage_is_a_real_threat(self):
  a=assault_transit(0,4);b=assault_transit(2000,4);self.assertLess(b['hp']+b['shield'],a['hp']+a['shield'])
 def test_assault_design(self):
  s=CFG['ships']['assault'];self.assertEqual(s['size'],1);self.assertEqual(s['pd'],1);self.assertEqual(s['weapons'],[])
 def test_other_hulls_and_xl_weapons_preserved(self):
  old=json.loads((R.parent/'FleetEndurance/profile.json').read_text())
  self.assertEqual(CFG['weapons'],old['weapons']);self.assertEqual(CFG['tier_damage'],old['tier_damage'])
  for k in old['ships']:
   if k!='assault':self.assertEqual(CFG['ships'][k],old['ships'][k])
 def test_results_and_provenance(self):
  r=json.loads((R/'results.json').read_text());self.assertEqual([len(r[k]) for k in ('grid','old_conditions','pods','transit')],[3840,320,960,512]);self.assertEqual(r['study_sha256'],hashlib.sha256((R/'study.py').read_bytes()).hexdigest());self.assertEqual(r['profile_sha256'],hashlib.sha256((R/'profile.json').read_bytes()).hexdigest())
  for v in r['grid_summary']:self.assertEqual(v['attacker_wins']+v['defender_wins']+v['mutual']+v['timeouts'],v['n'])
 def test_linked_survivors_keep_actual_roles(self):
  r=json.loads((R/'linked_results.json').read_text());self.assertEqual(len(r['trials']),192)
  for x in r['trials'][:4]:
   wave=pod_wave(heroes(4,'balanced')+['soldier']*6,x['pd'],x['seed']);self.assertEqual(x['attacker_roles'],[p['role'] for p in wave['pods'] if p['state']=='arrived'])
 def test_scale_controls_really_preserve_budget(self):
  r=json.loads((R/'scale_results.json').read_text());self.assertEqual(len(r['trials']),96)
  base=next(x for x in r['trials'] if x['case']=='original' and x['budget']==100)
  for n in (2,3):
   x=next(x for x in r['trials'] if x['case']=='replicate100' and x['budget']==100*n);self.assertEqual(x['hull'],base['hull']*n);self.assertAlmostEqual(x['nominal_dps'],base['nominal_dps']*n)
if __name__=='__main__':unittest.main()
