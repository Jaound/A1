"""Rules checks for the reduced model; these do not validate Unreal code."""
import copy,random,unittest
from engine import P,Ship,FleetBattle,HeroBattle,HybridBattle,add,mul,norm,sub

class CombatRules(unittest.TestCase):
 def ship(self,kind='cruiser'):return Ship(kind,0,0,(0,0,0),random.Random(3))
 def test_active_shield_never_regenerates_even_out_of_combat(self):
  s=self.ship();s.shield=s.d['shield']*.6;s.tick_shield(100,1000);self.assertEqual(s.shield,s.d['shield']*.6)
 def test_broken_shield_waits_and_restarts_at_half(self):
  s=self.ship();s.shield=0;s.shield_active=False;s.recovery=6.5;s.last_hit=0
  s.tick_shield(6,6);self.assertEqual(s.shield,0)
  s.tick_shield(.5,6.5);self.assertEqual(s.shield,0)
  for n in range(1,30):s.tick_shield(1,6.5+n)
  self.assertTrue(s.shield_active);self.assertEqual(s.shield,s.d['shield']*.5)
 def test_occupied_shield_cannot_restart(self):
  s=self.ship();s.disabled['shield']='occupied';s.tick_shield(100,1000);self.assertEqual(s.shield,0);self.assertFalse(s.shield_active)
 def test_core_loss_wrecks_without_zero_hull(self):
  s=self.ship();queue=[];s.kill_room(s.facility_keys['core'],'sabotage',queue);self.assertTrue(s.dead);self.assertGreater(s.hp,0)
 def test_internal_damage_enters_hull_once(self):
  s=self.ship();before=s.hp;s.internal_damage(s.facility_keys['engine'],100,'external',[]);self.assertAlmostEqual(before-s.hp,65)
 def test_empty_slot_still_causes_hull_damage(self):
  s=self.ship();key=next(k for k,v in s.slots.items() if v['type']=='empty');before=s.hp;s.internal_damage(key,100,'external',[]);self.assertEqual(before-s.hp,100)
 def test_shield_overflow_is_fractional_and_not_double_counted(self):
  s=self.ship();s.shield=20;s.armor['front']=[0];before=s.hp;s.damage(120,(s.bounds()[0],0,0),1)
  self.assertAlmostEqual(before-s.hp,100*.35*.65)
 def test_weapon_supply_and_pd_are_independent(self):
  s=self.ship();s.disabled['weapon']='occupied';self.assertFalse(any(s.available(w) for w in s.weapons));self.assertTrue(s.functional('pd'))
  s.disabled.clear();s.disabled['ammo']='destroyed';self.assertFalse(s.available(next(w for w in s.weapons if w['kind']=='cannon')));self.assertTrue(s.available(next(w for w in s.weapons if w['kind']=='beam')))
 def test_tracking_information_selects_best_effective_bonus(self):
  b=FleetBattle(['cruiser','cruiser','radar'],['cruiser'],seed=1,distance=600,motion='fixed');s=b.ships[0];t=b.ships[-1];s.disabled['radar']='destroyed'
  self.assertAlmostEqual(b.radar(s,t),.16*.8)
  b.ships[2].disabled['radar']='destroyed';self.assertEqual(b.radar(s,t),0)
  b.ships[1].disabled['radar']='destroyed';self.assertIsNone(b.radar(s,t))
 def test_lost_radar_forbids_new_long_range_fire(self):
  b=FleetBattle(['cruiser'],['cruiser'],seed=1,distance=900,motion='fixed')
  for s in b.ships:s.disabled['radar']='destroyed'
  for _ in range(100):b.step(.1)
  self.assertEqual(b.metrics['shots'],0)
 def test_already_fired_round_still_hits_captured_target(self):
  b=FleetBattle(['frigate'],['frigate'],seed=1,distance=200,motion='fixed');source,target=b.ships
  for s in b.ships:s.weapons=[];s.pd_timers=[];s.pos=(s.pos[0],0,0);s.prev=s.pos
  target.side=0;target.shield=0;target.shield_active=False;before=target.hp
  b.projectiles=[{'kind':'cannon','owner':source.idx,'side':0,'target':target,'pos':(0,0,0),'direction':(1,0,0),'speed':1000,'damage':100,'hp':None,'expiry':5}]
  b.step(.1);self.assertLess(target.hp,before)
 def test_non_guided_round_does_not_change_direction(self):
  b=FleetBattle(['frigate'],['frigate'],seed=1,distance=700,motion='fixed')
  for s in b.ships:s.weapons=[];s.pd_timers=[]
  p={'kind':'missile','owner':0,'side':0,'target':b.ships[1],'pos':(0,200,0),'direction':(1,0,0),'speed':100,'damage':100,'hp':24,'evasion':.25,'expiry':10};b.projectiles=[p];b.step(.1);self.assertEqual(p['direction'],(1,0,0))
 def test_pd_from_surface_intercepts_outside_center_radius(self):
  b=FleetBattle(['dreadnought'],['frigate'],seed=3,motion='fixed');ship=b.ships[0]
  for v in b.ships:v.weapons=[]
  ship.pd_timers=[0]*8;b.rng.random=lambda:0
  origin=b.pd_origins(ship)[0];point=add(origin,(30,0,0));self.assertGreater(norm(sub(point,ship.pos)),111)
  b.projectiles=[{'kind':'missile','owner':1,'side':1,'target':ship,'pos':point,'direction':(-1,0,0),'speed':0,'damage':100,'hp':24,'evasion':.25,'expiry':5}]
  b.step(.1);self.assertEqual(b.metrics['pd_kills'],1)
 def test_adjacent_supply_chain_explodes_once_per_room(self):
  ship=self.ship('corvette');ship.shield=0;ship.shield_active=False
  for face in ship.armor:ship.armor[face]=[0]*len(ship.armor[face])
  ship.slots[ship.facility_keys['missile']]={'type':'empty','hp':0}
  ship.slots[(0,1,0)]={'type':'ammo','hp':1};ship.slots[(0,2,0)]={'type':'missile','hp':1}
  core=ship.slots[ship.facility_keys['core']];core['hp']=100
  w=copy.deepcopy(ship.weapons[0]);w['kind']='missile';w['tier']=1;ship.weapons.append(w)
  rx,ry,rz=ship.bounds();ship.damage(10,(-.4*rx,-ry,0),1)
  self.assertEqual(core['hp'],80);self.assertFalse(ship.dead)
  self.assertIn('ammo',ship.disabled);self.assertIn('missile',ship.disabled)
 def test_three_deck_vertical_penetration_reaches_middle_only(self):
  ship=self.ship('battleship');ship.shield=0;ship.shield_active=False
  ship.armor['top']=[0]*9;core=ship.slots[ship.facility_keys['core']];before=core['hp'];rx,ry,rz=ship.bounds()
  ship.damage(50,(0,0,rz),1);self.assertEqual(before-core['hp'],50)
 def test_destroyed_bridge_cannot_be_captured(self):
  h=HybridBattle(3,'boarding_safe','bridge');h.entered=True;h.arrived=h.roles[:];h.phase='capturing';h.remaining=.01;h.walk_time=1
  h.target.slots[h.target.facility_keys['bridge']]['hp']=0;h.target.disabled['bridge']='destroyed'
  h.step(.1);self.assertFalse(h.objective);self.assertEqual(h.target.side,1);self.assertEqual(h.phase,'extracting')
 def test_finite_hero_healing_and_no_resurrection(self):
  b=HeroBattle(['support','melee'],['soldier']*16,seed=4);b.run();self.assertLessEqual(b.heal_total[0],P['heroes']['support']['heal']*4)
  dead=[u for u in b.units if not u.alive]
  for _ in range(100):b.step(.1)
  self.assertTrue(all(not u.alive for u in dead))
 def test_commander_aura_does_not_stack(self):
  b=HeroBattle(['ranged','commander','commander'],['turret'],seed=3)
  for u in b.units:u.x=0 if u.side==0 else 1;u.next=0;u.skill_next=1e9;u.damage=0 if u.role!='ranged' else 28
  target=b.units[-1];target.d['armor']=0;target.d['evasion']=0;b.rng.random=lambda:0
  before=target.hp;b.step(.1);self.assertAlmostEqual(before-target.hp,28*1.12)
 def test_wrecked_boarders_counted_only_once(self):
  h=HybridBattle(3,'boarding_safe');h.entered=True;h.phase='walking';h.arrived=h.roles[:];h.target.wreck()
  for _ in range(10):h.step(.1)
  self.assertEqual(len(h.dead),4)
 def test_returned_hero_dies_if_docking_ship_is_later_wrecked(self):
  h=HybridBattle(3,'boarding_safe');h.safe_home=['melee'];h.hero_homes={'melee':h.host};h.entered=True;h.launched_at=1;h.sent=4;h.done=True;h.phase='returned';h.host.wreck()
  for _ in range(3):h.step(.1)
  self.assertFalse(h.safe_home);self.assertEqual(h.dead,['melee'])
 def test_fire_hold_releases_after_boarders_leave(self):
  h=HybridBattle(3,'boarding_safe');h.entered=True;h.phase='returned';h.done=True;h.target.hp=h.target.d['hp']*.4;h.b.hold_targets.add(h.target.idx)
  h.step(.1);self.assertNotIn(h.target.idx,h.b.hold_targets)
 def test_enemy_shield_blocks_pod_without_entry(self):
  h=HybridBattle(1,'boarding_safe');h.target.shield_active=True
  pod={'kind':'pod','owner':h.host.idx,'side':0,'target':h.target,'pos':(0,h.target.pos[1],h.target.pos[2]),'direction':(1,0,0),'speed':165,'damage':0,'hp':80,'evasion':.4,'expiry':5,'role':'melee'}
  for s in h.b.ships:s.weapons=[];s.pd_timers=[]
  h.b.projectiles=[pod];h.in_space=['melee']
  for _ in range(20):h.b.step(.1)
  self.assertFalse(h.arrived);self.assertIn('melee',h.rescue)

if __name__=='__main__':unittest.main()
