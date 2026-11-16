import unittest,random
from model import P,S,Ship,Battle,roster,cost
class Rules(unittest.TestCase):
 def test_costs_and_caps(self):
  for budget in (100,200,300):
   for kind in ('mixed','heavy','light','missile','carrier','logistics','no_pd','xl'):
    rs=roster(budget,kind)
    if rs is not None:self.assertEqual(cost(rs),budget);self.assertLessEqual(sum(P['ships'][x]['size']==4 for x in rs),budget//125)
  self.assertIsNone(roster(100,'xl'))
 def test_entire_catalog_appears(self):
  used=set()
  for budget in (100,200,300):
   for kind in ('mixed','heavy','light','missile','carrier','logistics','no_pd','xl'):used.update(roster(budget,kind) or [])
  self.assertEqual(used,set(P['ships']))
 def test_hangar_fighter_counts(self):
  for k in ('carrier','battlecarrier'):self.assertEqual(P['ships'][k]['fighters'],20*P['ships'][k]['hangars'])
 def test_active_shield_does_not_heal(self):
  b=Battle(['frigate'],['frigate']);s=b.ships[0];s.shield=10
  for x in b.ships:x.guns=[]
  for _ in range(10):b.step()
  self.assertEqual(s.shield,10)
 def test_hit_does_not_refresh_broken_shield_delay(self):
  s=Ship('frigate',0,0,random.Random(0));s.hit(s.shield+1,1);delay=s.delay;s.hit(1,4);self.assertEqual(s.delay,delay)
 def test_low_tier_penalty_applies_after_shield(self):
  a=Ship('battleship',0,0,random.Random(0));b=Ship('battleship',0,0,random.Random(0));a.hit(100,0,.15);b.hit(100,0,1);self.assertEqual(a.shield,b.shield);self.assertEqual(a.hp,b.hp)
  a.active=b.active=False;a.shield=b.shield=0;a.hit(100,1,.15);b.hit(100,1,1);self.assertGreater(a.hp,b.hp)
 def test_pd_cannot_damage_ship_without_attacks(self):
  b=Battle(['air_defense'],['air_defense'])
  for s in b.ships:s.guns=[]
  before=[s.hp+s.shield for s in b.ships]
  for _ in range(10):b.step()
  self.assertEqual(before,[s.hp+s.shield for s in b.ships])
 def test_repair_is_finite_and_never_resurrects(self):
  b=Battle(['repair','frigate'],['frigate']);repair,patient,_=b.ships;patient.hp-=100;repair.repair_pool=10
  for s in b.ships:s.guns=[]
  b.step();self.assertEqual(repair.repair_pool,0);self.assertAlmostEqual(patient.hp,patient.maxhp-90)
  patient.dead=True;hp=patient.hp;b.step();self.assertEqual(patient.hp,hp)
 def test_carrier_wreck_stops_fighters(self):
  s=Ship('carrier',0,0,random.Random(0));s.active=False;s.hit(s.maxhp*100,1);self.assertTrue(s.dead);self.assertEqual(s.fighters,0)
 def test_pending_attack_survives_attacker_loss_and_capture(self):
  b=Battle(['frigate'],['frigate','frigate']);source,target,_=b.ships
  for s in b.ships:s.guns=[]
  source.dead=True;target.side=0;before=target.shield;b.pending=[[1,target,100,'cannon',0,1,1]];b.step();self.assertLess(target.shield,before)
 def test_retake_does_not_revive_wreck(self):
  b=Battle(['frigate'],['frigate']);target=b.ships[1];target.dead=True;b.retakes=[(1,target,0)];b.step();self.assertTrue(target.dead);self.assertEqual(target.side,1)
 def test_captured_home_not_counted_as_friendly_survival(self):
  b=Battle(['assault'],['dreadnought','cruiser'],policy='capture');home=b.ships[1];home.side=0
  for s in b.ships:s.guns=[];s.fighters=0;s.d=dict(s.d,pd=0)
  b.board={'target':home,'ship':b.ships[0],'phase':'return_flight','remaining':.1,'heroes':4,'return_pods':[240]*4,'exposure':0};b.retakes=[(2,home,1)]
  r=b.run(limit=3);self.assertEqual(r.get('heroes_alive_end',0),0);self.assertEqual(r['heroes_captive_or_hostile_aboard_end'],4)
if __name__=='__main__':unittest.main()
