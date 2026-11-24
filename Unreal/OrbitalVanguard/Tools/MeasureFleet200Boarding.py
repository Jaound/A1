"""200코스트 PIE에서 같은 편성의 영웅·일반병과 운송수단을 비교한다."""
from collections import Counter
import json
from pathlib import Path
import re
import traceback
import unreal as u

def arg(name, default):
    match = re.search(r"-OV" + name + r"=([\w,-]+)", u.SystemLibrary.get_command_line())
    return match.group(1) if match else default

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / ("Fleet200Boarding_" + arg("Tag","study") + ".json")
result = {"runs": [], "step": .1}
def save():
    out.parent.mkdir(exist_ok=True, parents=True)
    out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")

def p(obj, key):
    return obj.get_editor_property(key)

def enum(value):
    return value.name

def trial(pc, seed, mode):
    world = editor.get_game_world()
    stock = mode.startswith("stock")
    controlled = mode.startswith("close")
    for name, value in (("FleetSeed",seed),("Fleet200Shuttle",int(not stock))):
        u.SystemLibrary.execute_console_command(world, "ov.Trial.%s %s" % (name,value),pc)
    if not pc.start_play_scenario("Fleet200"):
        raise RuntimeError(str(p(pc,"last_action")))
    play = pc.get_play_scenario()
    ships = list(p(pc,"ships"))
    names = {"dreadnought": "구축전함", "assault": "강습선", "air_defense": "방공함"}

    def find(side, hull):
        matches = [ship for ship in ships if p(ship, "friendly") == side
                   and names[hull] in p(ship, "ship_name")]
        if len(matches) != 1:
            raise ValueError(f"{names[hull]} 식별 실패 (소속 {side}, 후보 {len(matches)})")
        return matches[0]

    home, enemy = find(True,"dreadnought"), find(False,"dreadnought")
    shuttle = None if stock else find(True,"assault")
    interior, transit = p(pc,"interior"), p(pc,"transport")
    target_sys, origin_sys = p(enemy,"systems"),p(home,"systems")
    bridge = next(p(room,"slot_id") for room in p(p(target_sys,"layout"),"slots") if p(room,"facility")==u.OVFacilityType.BRIDGE)
    troops = list(interior.register_boarding_team(home,0,0,8))
    heroes = [p(v,"unit_id") for v in p(interior,"units") if p(v,"ship")==home and p(v,"role")!=u.OVInteriorRole.TROOPER]
    hero_team = mode.endswith("hero")
    team = (heroes[:4]+troops[:4]) if hero_team else troops
    if hero_team and "troopfirst" in mode:
        team=troops[:4]+heroes[:4]
    assert len(team)==8 and (not hero_team or len(heroes)>=4), (mode,len(troops),len(heroes))
    # Transport settings persist across scenario resets on the same world subsystem.
    original_pod_hp=p(p(transit,"settings"),"pod_hp")
    people = {p(interior.get_unit(i),"person_id"):i for i in team}
    board = not mode.endswith("hold")
    via_ship = "shuttle" in mode
    combat=p(pc,"combat")
    if shuttle:
        assert not [v for v in p(interior,"units") if p(v,"ship")==shuttle], "비교 강습선은 빈 상태에서 시작"
    if controlled:
        # 저항을 분리하는 접촉 시험. 일반 함대전 표본과 별도로 보고한다.
        for ship in ships:
            ship.set_automation(False,False)
            ship.stop_movement()
        home.set_actor_location(enemy.get_actor_location()+u.Vector(-17500,0,0), False, False)
        guard=re.search(r"guard(80|120|160|200)",mode)
        if guard:
            # 동일한 200코스트 편성의 방공함을 포드 접근로 옆에 호위 배치한다.
            defense_ship=find(False,"air_defense")
            position=enemy.get_actor_location()
            home.set_actor_location(position+u.Vector(-23500,0,0),False,False)
            defense_ship.set_actor_location(position+u.Vector(-14500,5500,0),False,False)
            kinds=u.OVPDTargetKind
            p(pc,"point_defense").set_priority(defense_ship,[kinds.POD,kinds.MISSILE,kinds.AIRCRAFT])
            settings=p(transit,"settings")
            settings.set_editor_property("pod_hp",float(guard.group(1)))
            transit.set_editor_property("settings",settings)
        if "shield" not in mode:
            for room in p(p(target_sys,"layout"),"slots"):
                if p(room,"facility")==u.OVFacilityType.SHIELD_CONTROL:
                    target_sys.debug_set_crew(p(room,"slot_id"),0)
            assert not p(target_sys,"shield_active")
    home.set_automation(False,not controlled)
    initial_costs=list(p(play,"initial_costs"))
    assert initial_costs==[200,200],initial_costs
    record={"mode":mode,"seed":seed,"costs":initial_costs,"heroes_at_origin":len(heroes),
            "team_heroes":4 if hero_team and board else 0,"team_sent_at":None,"first_entry":None,
            "capture":None, "first_shield_down":None,"first_in_range":None,"min_range_m":1e6,
            "shuttle_changes":[], "entered":0}
    defenders=[p(v,"unit_id") for v in p(interior,"units") if p(v,"ship")==enemy and p(v,"faction_id")==1]
    record["initial_defenders"]=len(defenders)
    if via_ship:
        hull=p(shuttle,"systems")
        record["shuttle_start_hp_shield"]=[round(p(hull,"hull_hp")),round(p(hull,"shield"))]
        record["flight_min_hp_shield"]=list(record["shuttle_start_hp_shield"])
    entered=set()
    pd_hits={}
    defense=p(pc,"point_defense")
    auto_paused=p(pc,"simulation_paused")
    if auto_paused: pc.action("Pause")
    assert abs(p(pc,"simulation_speed")-1)<.001, "1배속 비교"
    prepared=pc.prepare_boarding(home,enemy,shuttle if via_ship else None,team) if board else False
    record["prepared"]=prepared
    if board and not prepared:record["prepare_message"]=str(p(pc,"last_action"))
    serial=pc.last_crew_operation()
    limit=int(arg("Seconds","180"))
    for sec in range(limit+1):
        hpos,epos=home.get_actor_location(),enemy.get_actor_location()
        dist=(hpos-epos).length()/100
        record["min_range_m"]=round(min(record["min_range_m"],dist),1)
        if dist<=250 and record["first_in_range"] is None:record["first_in_range"]=sec
        shield=p(target_sys,"shield_active")
        if not shield and record["first_shield_down"] is None:record["first_shield_down"]=sec
        if not controlled and sec%3==0 and not p(origin_sys,"wrecked") and not p(target_sys,"wrecked"):
            home.set_destination(epos+u.Vector(-14000,0,0))
        launch_window = dist <= 245 and (not shield or "shield" in mode)
        if mode.startswith("earlyshuttle"):
            launch_window = True
        if prepared and record["team_sent_at"] is None and launch_window and not p(target_sys, "wrecked"):
            if pc.confirm_boarding(serial):
                record["team_sent_at"]=sec
            else:record["last_block"]=str(p(pc,"last_action"))
        if via_ship and record["team_sent_at"] is not None and not entered:
            record["flight_min_hp_shield"]=[min(old,round(p(hull,key))) for old,key in zip(record["flight_min_hp_shield"],("hull_hp","shield"))]
            if sec==record["team_sent_at"] or sec%10==0:
                separation = shuttle.get_actor_location() - enemy.get_actor_location()
                record.setdefault("approach_samples", []).append({
                    "second": sec,
                    "target_distance_m": round(separation.length() / 100, 1),
                    "engine": round(hull.get_efficiency(u.OVFacilityType.ENGINE), 3),
                    "bridge": round(hull.get_efficiency(u.OVFacilityType.BRIDGE), 3),
                    "neutral": p(hull, "neutral"),
                    "can_move": hull.can_move(),
                    "hp": round(p(hull, "hull_hp")),
                    "shield": round(p(hull, "shield")),
                })
            if "focused" in mode and not p(hull,"wrecked"):
                if combat.set_target(enemy,shuttle):
                    if combat.request_fire(enemy,"None"):
                        record["focus_requests"]=record.get("focus_requests",0)+1
        for event in p(transit,"entry_events"):
            pid=p(event,"person_id")
            if pid in people and pid not in entered and p(event,"target")==enemy:
                entered.add(pid)
                if record["first_entry"] is None:record["first_entry"]=round(p(event,"time"),2)
                interior.issue_order([people[pid]],u.OVInteriorOrder.CAPTURE,bridge)
        if not p(target_sys,"wrecked") and p(target_sys,"faction_id")==0 and record["capture"] is None:
            record["capture"] = sec
        if prepared and record["team_sent_at"] is not None and not via_ship and sec-record["team_sent_at"]<=30:
            pod_ids={p(x,"pod_id") for x in p(transit,"pods") if p(x,"person_id") in people}
            for ev in p(defense,"recent_events"):
                if p(ev,"target_kind")==u.OVPDTargetKind.POD and p(ev,"target_id") in pod_ids:
                    pd_hits[p(ev,"event_id")]=(p(ev,"hit"),p(ev,"destroyed"))
        if via_ship:
            sh=next((s for s in p(transit,"shuttles") if p(s,"ship")==shuttle),None)
            if sh:
                record["last_shuttle_reason"]=str(p(sh,"reason"))
                state=enum(p(sh,"state"))
                if not record["shuttle_changes"] or record["shuttle_changes"][-1][1]!=state:
                    record["shuttle_changes"].append([sec,state,str(p(sh,"reason"))])
        if p(play,"result")!=u.OVPlaySceneResult.RUNNING or sec==limit:break
        if controlled and record["capture"] is not None:break
        if "full" not in mode:
            if p(origin_sys,"wrecked") and record["team_sent_at"] is None:break
            if p(target_sys,"wrecked"):break
        for _ in range(10):
            pc.step_simulation(.1)
    pods=[x for x in p(transit,"pods") if p(x,"person_id") in people]
    record.update(pd_shots_seen=len(pd_hits),
                  pd_hits_seen=sum(hit for hit, _ in pd_hits.values()),
                  pd_kills_seen=sum(destroyed for _, destroyed in pd_hits.values()),
                  elapsed=round(p(play, "elapsed_seconds"), 1), entered=len(entered),
                  pod_launched=sum(p(x,"launch_time")>=0 for x in pods),
                  pod_states=dict(Counter(enum(p(x,"state")) for x in pods)),
                  people=dict(Counter(enum(p(transit.get_person(i),"state")) for i in people)),
                  target_hull_pct=round(100*p(target_sys,"hull_hp")/p(target_sys,"max_hull_hp"),1),
                  origin_wrecked=p(origin_sys,"wrecked"),target_wrecked=p(target_sys,"wrecked"),
                  result=enum(p(play,"result")))
    record["defenders_killed"]=sum(p(interior.get_unit(i),"hp")<=0 for i in defenders)
    record["defenders_hp_remaining"]=round(sum(max(0,p(interior.get_unit(i),"hp")) for i in defenders))
    record["salvos_or_shots"]=p(p(pc,"combat"),"shot_count")
    if shuttle:
        sy=p(shuttle,"systems")
        record["shuttle_hull"]=[round(p(sy,"hull_hp")),round(p(sy,"max_hull_hp"))]
        record["shuttle_wrecked"]=p(sy,"wrecked")
    record["surviving_fleet"]=[sum(not p(p(s,"systems"),"wrecked") and p(p(s,"systems"),"faction_id")==side for s in ships) for side in (0,1)]
    record["pod_hp_profile"]=p(p(transit,"settings"),"pod_hp")
    record["pod_hp_at_end"]=[round(p(x,"hp")) for x in pods]
    if "guard" in mode:
        settings=p(transit,"settings")
        settings.set_editor_property("pod_hp",original_pod_hp)
        transit.set_editor_property("settings",settings)
    if not p(pc,"simulation_paused"):pc.action("Pause")
    return record

def run():
    while not editor.get_game_world():
        yield
    world=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(world,0)
    while not pc or not pc.get_hud():
        yield
        pc=u.GameplayStatics.get_player_controller(world,0)
    pc.get_hud().close_title_screen()
    for seed in map(int,arg("Seeds","260112,260113,260114").split(",")):
        for mode in arg("Cases","stockhold,stockpodhero,hold,podtroop,podhero,shuttletroop,shuttlehero").split(","):
            result["current"]=[seed,mode]
            save()
            record=trial(pc,seed,mode)
            result["runs"].append(record)
            save()
            yield

def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        result["complete"]=True
    except Exception:
        result["error"]=traceback.format_exc()
    save()
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
