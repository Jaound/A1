"""같은 300코스트 편성과 시드에서 두 거리의 미사일 교전 및 적 영웅 운용을 비교한다."""
import json
import traceback
from pathlib import Path

import unreal as u

ed = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation/CommandAI.json"
report = {"seed": 260112, "seconds_per_case": 20, "cases": [], "assets_seen": []}
def prop(obj, name):
    return obj.get_editor_property(name)

def run():
    while not ed.get_game_world():
        yield
    pc = u.GameplayStatics.get_player_controller(ed.get_game_world(), 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(ed.get_game_world(), 0)
    play, combat, interior = pc.get_play_scenario(), prop(pc, "combat"), prop(pc, "interior")
    for distance in (26000, 44000):
        for level in (u.OVCommandDifficulty.EASY, u.OVCommandDifficulty.NORMAL, u.OVCommandDifficulty.HARD):
            play.set_editor_property("next_difficulty", level)
            assert pc.start_play_scenario("Fleet300")
            assert prop(play, "active_difficulty") == level
            ships = list(prop(pc, "ships"))
            for ship in ships:
                ship.set_automation(False, False)
                ship.stop_movement()
            def find(hull, friendly):
                return next(ship for ship in ships if {"missile_ship":"전략", "corvette":"초계함", "bastion":"바스티온"}[hull] in prop(ship,"ship_name") and prop(ship,"friendly")==friendly)
            # 이격된 두 사선을 서로 반대로 배치해 양측 자동 회피와 두 종류의 메시를 함께 확인한다.
            pairs = [(find("missile_ship", True), find("corvette", False)),
                     (find("missile_ship", False), find("corvette", True)),
                     (find("bastion", True),find("bastion", False))]
            start = u.Vector(0, 600000, 0)
            for lane,(source,target) in enumerate(pairs):
                loc = start + u.Vector(0, lane*155000, 0)
                source.set_actor_location(loc, False, False)
                source.set_actor_rotation(u.Rotator(0,0,0),False)
                target.set_actor_location(loc+u.Vector(distance,0,0),False,False)
                source.set_automation(False,False)
                target.set_automation(True,False)
                assert combat.set_target(source,target)
                assert combat.request_fire(source,"None")
            pc.action("Pause")
            seen, hits = set(), set()
            aimed_at_enemy, enemy_hits = set(), set()
            victim=pairs[0][1]
            victim_systems=prop(victim,"systems")
            def health():
                return round(prop(victim_systems,"hull_hp")+prop(victim_systems,"shield"),1)
            initial_health=health()
            for _ in range(80):
                pc.step_simulation(.25)
                pc.update_combat_presentation()
                for kind in ("standard_missile_meshes","anti_ship_missile_meshes"):
                    comp=prop(pc,kind)
                    if comp and comp.get_instance_count():
                        mesh=prop(comp,"static_mesh").get_path_name()
                        if mesh not in report["assets_seen"]: report["assets_seen"].append(mesh)
                for shot in prop(combat,"attacks"):
                    if prop(prop(shot,"weapon"),"family")==u.OVWeaponType.MISSILE:
                        seen.add(prop(shot,"attack_id"))
                        if prop(shot,"attacker")==pairs[0][0] and prop(shot,"target")==victim:
                            aimed_at_enemy.add(prop(shot,"attack_id"))
                for event in prop(combat,"recent_events"):
                    if prop(event,"attack_id") in seen and prop(event,"result")==u.OVAttackResult.HIT:
                        hits.add(prop(event,"attack_id"))
                        if prop(event,"attack_id") in aimed_at_enemy and str(prop(event,"hit_ship_id"))==victim.get_name():
                            enemy_hits.add(prop(event,"attack_id"))
            heroes=[h for h in prop(interior,"units") if prop(h,"faction_id")==1 and prop(h,"role")!=u.OVInteriorRole.TROOPER]
            row={"difficulty":level.name,"range_m":distance/100,"tracked_missiles":len(seen),"hits":len(hits),
                 "enemy_target_missiles":len(aimed_at_enemy),"enemy_target_hits":len(enemy_hits),
                 "target_start_hp_shield":initial_health,"target_end_hp_shield":health(),
                 "enemy_evades":prop(play,"enemy_evasions"),"ally_evades":prop(play,"allied_evasions"),
                 "enemy_heroes":len(heroes),"active_duties":sum(prop(h,"duty_active") for h in heroes),
                 "defend_orders":sum(prop(h,"order")==u.OVInteriorOrder.DEFEND for h in heroes)}
            report["cases"].append(row)
            out.write_text(json.dumps(report,indent=2),encoding="utf-8")
            row["enemy_definitions"]=sorted({str(prop(h,"definition_id")) for h in heroes})
            assert set(row["enemy_definitions"])=={"G01","G02","G03","G04"},row
            assert len(heroes)==15 and row["defend_orders"] and row["active_duties"] and seen, row
            yield
    assert any(row["enemy_evades"] for row in report["cases"] if row["difficulty"]=="HARD")
    assert all(row["enemy_evades"]==0 for row in report["cases"] if row["difficulty"]=="EASY")
    assert all(row["ally_evades"]>0 for row in report["cases"])
    assert len(report["assets_seen"])==2, report["assets_seen"]
    assert pc.start_play_scenario("Fleet300")
    ships=list(prop(pc,"ships"))
    for ship in ships:
        ship.set_automation(False, False)
        ship.stop_movement()
    eligible=[h for h in prop(interior,"units") if prop(h,"faction_id")==1 and prop(h,"role")==u.OVInteriorRole.MELEE
              and prop(prop(h,"ship"),"systems").get_efficiency(u.OVFacilityType.BOARDING)>0]
    assert eligible, "강습 시설을 가진 적 돌격 영웅이 없음"
    hero=eligible[0]
    unit_id=prop(hero,"unit_id")
    source=prop(hero,"ship")
    victim=find("corvette",True)
    source.set_actor_location(u.Vector(0,900000,0),False,False)
    victim.set_actor_location(u.Vector(14000,900000,0),False,False)
    assert combat.set_target(source,victim)
    transport=prop(pc,"transport")
    system=prop(victim,"systems")
    assert prop(system,"shield_active")
    pc.action("Pause")
    for _ in range(8): pc.step_simulation(.25)
    assert not any(prop(pod,"origin")==source for pod in prop(transport,"pods"))
    # 시설과 승객은 정상 상태. 외부 피격 경로로 표적 쉴드만 내린 뒤 AI가 스스로 결정하게 한다.
    system.apply_world_hit(victim.get_actor_location(),u.Vector(0,0,1),prop(system,"shield")+1,u.OVDamageCause.EXTERNAL)
    arrived=False
    for _ in range(48):
        pc.step_simulation(.25)
        live=next(h for h in prop(interior,"units") if prop(h,"unit_id")==unit_id)
        if prop(live,"ship")==victim and prop(live,"order")==u.OVInteriorOrder.CAPTURE:
            arrived=True
            break
    report["enemy_boarding"]={"shield_held":True,"pods":sum(prop(pod,"origin")==source for pod in prop(transport,"pods")),
                              "arrival_and_capture_order":arrived}
    assert report["enemy_boarding"]["pods"]>0 and arrived, report["enemy_boarding"]
    report["passed"]=True

def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        pass
    except Exception:
        report["error"]=traceback.format_exc()
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
