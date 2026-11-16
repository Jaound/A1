import copy
import json
import random
import unittest
from pathlib import Path

from model import Battle, Ship, P, C, cost, roster
from indoor import Unit, Layout, indoor_step


class Rules(unittest.TestCase):
    def battle(self,**kwargs):return Battle(['assault','battleship','repair'],['battleship'],7,heroes=False,**kwargs)

    def test_shield_active_never_regenerates(self):
        s=self.battle().ships[0];s.shield=100;s.shield_tick(999,50);self.assertEqual(s.shield,100)

    def test_hit_does_not_reset_regeneration(self):
        b=self.battle();s=b.ships[1];s.active=False;s.shield=0
        for t in range(10):b.t=t;b.hit(s,1,1);s.shield_tick(t,1)
        self.assertAlmostEqual(s.shield,s.maxshield*C['combat_regen']*10)

    def test_crew_changes_regeneration(self):
        s=self.battle().ships[1];s.active=False;s.shield=0;s.last_combat=0;s.facilities['shield']['crew']=.5
        s.shield_tick(1,1);self.assertAlmostEqual(s.shield,s.maxshield*C['combat_regen']*.5)

    def test_noncombat_rate_and_half_restart(self):
        s=self.battle().ships[1];s.active=False;s.shield=0
        s.shield_tick(99,100);self.assertTrue(s.active);self.assertEqual(s.shield,s.maxshield*.5)

    def test_destroyed_shield_cannot_recover(self):
        b=self.battle();s=b.ships[1];b.destroy_facility(s,'shield');s.shield_tick(99,100)
        self.assertEqual(s.shield,0);self.assertFalse(s.active)

    def test_weapon_overkill_never_damages_hull(self):
        b=self.battle();s=b.ships[1];s.active=False;s.shield=0;before=s.hp
        b.hit(s,1e9,1,weapon_index=0);b.hit(s,100,1,weapon_index=0)
        self.assertEqual(s.hp,before);self.assertEqual(s.guns[0]['hp'],0)

    def test_facility_crew_independent_from_hp(self):
        s=self.battle().ships[1];s.facilities['weapon']['crew']=0
        self.assertGreater(s.facilities['weapon']['hp'],0);self.assertFalse(s.combat_ready())

    def test_friendly_boarders_protect_enemy_target(self):
        b=self.battle();s=b.ships[-1];u=b.add_unit(s,'melee',0);u.state='inside';b.observe()
        self.assertTrue(b.protected(s,0));self.assertIsNone(b.pick(b.ships[1]))
        u.state='transit';self.assertFalse(b.protected(s,0))

    def test_already_fired_hit_survives_capture(self):
        b=self.battle();s=b.ships[-1];s.active=False;s.shield=0
        b.schedule(0,{'kind':'shot','target':s,'side':0,'damage':100,'weapon':'beam','beam_hit':True,'gun':None,'source_size':3})
        b.change_owner(s,0);before=s.hp;b.process_events();self.assertLess(s.hp,before)

    def test_capture_clears_orders_not_health_or_units(self):
        b=self.battle();s=b.ships[-1];s.hp-=100;u=s.units[0];before=s.hp;b.change_owner(s,0)
        self.assertEqual(s.hp,before);self.assertEqual(u.side,1);self.assertIsNone(s.target)

    def test_observation_does_not_leak_hidden_data(self):
        b=self.battle();b.observe();before=copy.deepcopy(b.knowledge)
        b.ships[-1].cargo=[9999,9999,9999];b.add_unit(b.ships[-1],'support',1,20,4);b.observe()
        self.assertEqual(before,b.knowledge)
        for contact in b.knowledge[0].values():self.assertNotIn('cargo',contact);self.assertNotIn('heroes',contact)

    def test_lost_contact_prevents_new_targeting(self):
        b=self.battle();b.ships[-1].pos=[1e6,0,0];b.observe();self.assertFalse(b.knowledge[0]);self.assertIsNone(b.pick(b.ships[1]))

    def test_personal_pods_have_individual_events(self):
        b=Battle(['assault'],['battleship'],4,mode='pod',hero_budget=4,defenders=0)
        a,v=b.ships;a.pos=[0,0,0];v.pos=[150,0,0];v.active=False;v.shield=0;b.t=90;b.observe();b.launch_operation()
        events=sorted(x[0] for x in b.events);self.assertEqual(len(events),12)
        self.assertAlmostEqual(events[-1]-events[0],2.75)

    def test_assault_is_small_and_pd_immune(self):
        b=Battle(['assault'],['battleship'],4,mode='assault',hero_budget=4,defenders=0)
        a,v=b.ships;a.pos=[0,0,0];v.pos=[100,0,0];v.active=False;v.shield=0;b.t=90;b.observe();b.launch_operation()
        b.intercept=lambda *args: (_ for _ in ()).throw(AssertionError('PD attacked assault ship'))
        b.pod_arrive({'unit':b.operations[0]['units'][0],'op':b.operations[0],'returning':False,'direct':True})
        self.assertEqual(P['ships']['assault']['pd'],1);self.assertFalse(P['ships']['assault']['weapons'])

    def test_repair_actual_price_speed_and_no_crew_restore(self):
        b=self.battle();r=b.ships[2];s=b.ships[1];r.pos=list(s.pos);s.hp-=1000
        s.facilities['engine']['crew']=.4;b.repair_step(1);b.t=3;before=s.hp;stock=list(r.cargo);b.repair_step(1)
        restored=s.hp-before;self.assertAlmostEqual(restored,max(64,.001*s.maxhp)*.5)
        self.assertAlmostEqual(stock[0]-r.cargo[0],restored/s.maxhp*s.d['build_cost'][0]*.3*1.5)
        self.assertEqual(s.facilities['engine']['crew'],.4)

    def test_repair_cannot_use_missing_resource(self):
        b=self.battle();r=b.ships[2];s=b.ships[1];r.pos=list(s.pos);r.cargo=[999,999,0];s.hp-=1000
        b.repair_step(1);b.t=4;before=s.hp;b.repair_step(1);self.assertEqual(s.hp,before)

    def test_two_repair_ships_do_not_stack(self):
        b=Battle(['battleship','repair','repair'],['battleship'],heroes=False);s=b.ships[0];s.hp-=1000
        for r in b.ships[1:3]:r.pos=list(s.pos)
        b.repair_step(1);b.t=4;before=s.hp;b.repair_step(1)
        self.assertAlmostEqual(s.hp-before,79.2)

    def test_xl_weapons_unchanged(self):
        old=json.loads((Path(__file__).parent.parent/'FleetEndurance/profile.json').read_text())
        for key in ('dreadnought','battlecarrier'):
            self.assertEqual(P['ships'][key]['weapons'],old['ships'][key]['weapons']);self.assertEqual(P['ships'][key]['hp'],old['ships'][key]['hp'])
        self.assertEqual(P['weapons']['spinal'],old['weapons']['spinal'])

    def test_all_hulls_cost_and_exact_budgets(self):
        for n in (100,200,300):
            for kind in ('mixed','logistics','light','heavy','carrier','missile','xl'):self.assertEqual(cost(roster(n,kind)),n)
        self.assertEqual(set(P['ships']),{'corvette','frigate','vector','bastion','cruiser','air_defense','radar','battleship','carrier','missile_ship','dreadnought','battlecarrier','repair','mining','transport','assault'})

    def test_capacity_adds_slots_not_free_extra_initial_inventory(self):
        s=Ship('carrier',0,1,0,P,random.Random(1),capacity_rank=3)
        self.assertEqual(s.capacity,52)
        # Scenario explicitly purchases/fills its capacity; no runtime research effect creates units.
        s.fighters=40;self.assertEqual(s.capacity-s.fighters,12)

    def test_level_growth_and_legal_skill_budget(self):
        for lv in (1,5,10,15,20):
            u=Unit(1,'melee',0,0,P,lv)
            self.assertAlmostEqual(u.maxhp,300*(1+.06*(lv-1)))
            self.assertLessEqual(sum(x-1 for x in u.ranks.values()),sum(lv>=n for n in P['heroes']['skill_point_levels']))

    def test_support_consumable_capacity_not_free_refill(self):
        u=Unit(1,'support',0,0,P,20);self.assertEqual(u.medkits,4);self.assertEqual(u.tools,2)

    def test_wreck_kills_everyone_aboard(self):
        b=self.battle();s=b.ships[1];u=b.add_unit(s,'support',0);u.state='inside';b.wreck(s,'test')
        self.assertEqual(u.hp,0);self.assertEqual(u.state,'dead')

    def test_no_timer_only_recapture(self):
        b=self.battle();s=b.ships[-1];b.change_owner(s,0);s.units=[]
        for i in range(200):b.t=i;b.indoor(1)
        self.assertEqual(s.side,0)

    def test_real_bridge_occupation_recaptures(self):
        b=self.battle();s=b.ships[-1];b.change_owner(s,0);s.units=[];b.add_unit(s,'melee',1,node=s.layout.bridge)
        for i in range(35):b.t=i;b.indoor(1)
        self.assertEqual(s.side,0);b.t=35;b.indoor(1);self.assertEqual(s.side,1)

    def test_graph_deck_distance_and_core(self):
        a=Layout(3,60);self.assertEqual(a.core,22);self.assertEqual(a.distance(7,37),12)
        self.assertIn(22,a.edges[7]);self.assertNotIn(37,a.edges[7])

    def test_crew_zero_without_humans_neutralizes(self):
        b=self.battle();s=b.ships[1];s.units=[];s.officers=[]
        for f in s.facilities.values():f['crew']=0
        b.environment(1);self.assertEqual(s.side,-1)

    def test_same_seed_replays(self):
        a=Battle(['frigate'],['frigate'],4,heroes=False,duration=10).run()
        b=Battle(['frigate'],['frigate'],4,heroes=False,duration=10).run();self.assertEqual(a,b)

    def test_active_hero_count_is_faction_wide(self):
        b=Battle(roster(300),roster(300),mode='pod',level=20)
        self.assertEqual(b.initial_heroes,[15,15]);self.assertEqual(sum(u.hero for s in b.ships if s.side==0 for u in s.units+s.officers),15)

    def test_repaired_magazine_can_explode_in_a_new_event(self):
        b=self.battle();s=b.ships[1];b.destroy_facility(s,'ammo');before=b.metric['explosions']
        f=s.facilities['ammo'];f['hp']=f['maxhp'];f['destroyed']=False
        b.destroy_facility(s,'ammo');self.assertGreater(b.metric['explosions'],before)

    def test_external_facility_destruction_clears_linked_armor(self):
        b=self.battle();s=b.ships[1];before=copy.deepcopy(s.armor);b.destroy_facility(s,'weapon')
        self.assertNotEqual(s.armor,before);self.assertEqual(s.armor['front'],before['front'])

    def test_precise_sabotage_preserves_external_armor(self):
        b=self.battle();s=b.ships[1];before=copy.deepcopy(s.armor);b.destroy_facility(s,'weapon','sabotage')
        self.assertEqual(s.armor,before)

    def test_unfired_salvo_cancels_when_attacker_is_captured(self):
        b=self.battle();a,v=b.ships[1],b.ships[-1];a.pos=[-100,0,0];v.pos=[100,0,0]
        b.change_owner(a,1);b.change_owner(v,0);b.observe()
        b.schedule(0,{'kind':'launch','shot':{'attacker':a,'target':v,'side':0,'mount':a.guns[0],'flight':1}})
        b.process_events();self.assertEqual(b.metric['shots_launched'],0)

    def test_uncommanded_room_presence_is_not_automatic_capture(self):
        b=self.battle();s=b.ships[-1];s.units=[];f=s.facilities['engine']
        u=b.add_unit(s,'melee',0,node=f['node']);u.speed=0
        for i in range(20):b.t=i;b.indoor(1)
        self.assertEqual(f['owner'],1)

    def extraction_fixture(self):
        b=Battle(['assault','battleship'],['battleship'],4,mode='assault',hero_budget=4,defenders=0)
        source,target=b.ships[0],b.ships[-1];source.pos=[0,0,0];target.pos=[100,0,0]
        target.active=False;target.shield=0;b.t=90;b.observe();b.launch_operation()
        op=b.operations[0];b.t=op['arrival']+1;b.operations_step(1)
        op['objective']=True;op['phase']='return_walk'
        for u in op['units']:u.node=op['entry_node'];u.edge=None;u.state='return_walk'
        return b,source,target,op

    def test_assault_extraction_reuses_transport_not_new_pods(self):
        b,source,target,op=self.extraction_fixture();b.operations_step(1);b.t+=50;b.operations_step(1)
        self.assertEqual(b.metric['returned_heroes'],4)
        self.assertTrue(all(u in source.units for u in op['units']))
        self.assertFalse(any(e[2]['kind']=='pod' for e in b.events))

    def test_lost_transport_requires_emergency_preparation(self):
        b,source,target,op=self.extraction_fixture();b.wreck(source,'test');b.operations_step(1)
        self.assertEqual(b.metric['hero_rescue_events'],0)
        b.t+=15;b.operations_step(1);self.assertEqual(b.metric['hero_rescue_events'],4)
        self.assertTrue(all(u.state=='rescue' for u in op['units'] if u.hero))


if __name__=='__main__':unittest.main()
