"""실제 시험장에서 두 강습팀의 출발·접안·귀환을 비교한다."""
import json
import traceback
from pathlib import Path
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation/BoardingWaves.json"
result = {"checks": {}, "routes": {}}
def prop(obj, key):
    return obj.get_editor_property(key)
def check(key, condition, value):
    result["checks"][key] = {"passed": bool(condition), "value": str(value)}
    if not condition:
        raise AssertionError((key, value))

def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(world, 0)
    for route, label in enumerate(["포드", "강습선", "혼합"]):
        assert pc.start_play_scenario("HeroArena"), str(prop(pc,"last_action"))
        hud=pc.get_hud()
        hud.close_title_screen()
        for _ in range(4): yield
        width,height=pc.get_viewport_size()
        scale=max(.25,min(4,max(.25,min(width/1280,height/740))*hud.get_user_ui_scale_multiplier()))
        for button in ("ArenaFight","ArenaWaveB"):
            point=hud.get_action_center(button)
            check(label+" 패널 입력 "+button,str(hud.get_hit_action_at_canvas_point(point))==button and hud.is_over_panel(point.x*scale,point.y*scale),str(point))
        home, target = list(prop(pc, "ships"))
        carrier, transit, interior = [prop(pc, k) for k in ("carrier","transport","interior")]
        bays = lambda: [h for h in prop(carrier,"hangars") if prop(h,"carrier")==home]
        docked = lambda: [prop(h,"stored_small_ship") for h in bays() if prop(h,"stored_small_ship")]
        originals = set(docked())
        check(label+" 실제 모함 격납고",len(originals)==2,len(originals))
        for _ in range(route): pc.action("ArenaRoute")
        for _ in range(4): pc.action("ArenaLevel")
        pc.action("ArenaFill")
        own=[x for x in prop(interior,"units") if prop(x,"ship")==home and prop(x,"faction_id")==0]
        heroes=sorted(prop(x,"unit_id") for x in own if prop(x,"role")!=u.OVInteriorRole.TROOPER)
        soldiers=sorted(prop(x,"unit_id") for x in own if prop(x,"role")==u.OVInteriorRole.TROOPER)
        if route==0:
            pc.action("ArenaFormation")
            def move(unit,column):
                pc.action("ArenaPick_"+str(unit))
                pc.action("ArenaAssign_"+str(column))
            move(heroes[8],0)  # 정원 초과 시 여전히 예비
            move(heroes[0],2)
            move(heroes[9],0)
            move(soldiers[0],2)
            move(soldiers[16],0)
            move(heroes[4],2)
            move(heroes[0],1)
            pc.action("ArenaFormation")
        pc.action("ArenaFight")
        if route==0:
            actual={prop(transit.get_person(prop(pod,"person_id")),"runtime_unit_id") for pod in prop(transit,"pods")}
            expected=set(heroes[1:4]+[heroes[9]]+soldiers[4:8]+soldiers[16:20])
            check("지정 영웅·분대만 A팀 출발",actual==expected,sorted(actual))
        pc.action("ArenaWaveB")
        check(label+" 조기 후속 출발 차단",transit.active_teams_at(target)==1,transit.next_team_delay(target))
        for _ in range(610): pc.step_simulation(.05)
        pc.action("ArenaWaveB")
        observation={"활동팀":transit.active_teams_at(target),"명령":str(prop(pc,"last_action")),"강습선":[(str(prop(sh,"state")),str(prop(sh,"reason")),str(prop(sh,"ship").get_actor_location())) for sh in prop(transit,"shuttles")],"승객":{}}
        for person in prop(transit,"persons"):
            key=str(prop(person,"state"))
            observation["승객"][key]=observation["승객"].get(key,0)+1
        check(label+" 동일 목표 두 팀",transit.active_teams_at(target)==2,observation)
        for _ in range(1800):
            pc.step_simulation(.05)
            if len(prop(transit,"entry_events"))==24: break
        entries=[e for e in prop(transit,"entry_events") if prop(e,"target")==target and prop(e,"faction_id")==0]
        people=[prop(e,"person_id") for e in entries]
        ids=[prop(transit.get_person(i),"runtime_unit_id") for i in people]
        pods=list(prop(transit,"pods"))
        stats={"진입 인원":len(ids),"진입 완료 시 생존":len(interior.get_units_on_ship(target,0)),"포드":len(pods),"격납고 대기":len(docked()),"강습선 상태":[str(prop(sh,"state")) for sh in prop(transit,"shuttles")]}
        feed=hud.get_battle_notice_feed()
        notices=list(prop(feed,"notices"))
        arrivals=[n for n in notices if prop(n,"kind")==u.OVBattleNoticeKind.BOARDING_ENTRY]
        check(label+" 팀별 진입 알림",sorted((str(prop(n,"subject")),prop(n,"count")) for n in arrivals)==[("A팀",12),("B팀",12)],[(str(prop(n,"subject")),prop(n,"count")) for n in arrivals])
        if route==0:
            assigned_b={prop(transit.get_person(i),"runtime_unit_id") for i in people if prop(transit.get_person(i),"boarding_team_id")!=prop(transit.get_person(people[0]),"boarding_team_id")}
            check("A팀에서 옮긴 영웅은 B팀으로만 진입",heroes[0] in assigned_b and heroes[4] not in ids and heroes[8] not in ids,assigned_b)
        result["routes"][label]=stats
        check(label+" 실제 24명 진입",len(set(ids))==24,stats)
        num_heroes = sum(prop(interior.get_unit(i),"role")!=u.OVInteriorRole.TROOPER for i in ids)
        check(label+" 영웅·일반병 구성",num_heroes==8,num_heroes)
        check(label+" 원 수송수단 사용",len(pods)==[24,0,12][route] and len(docked())==[2,0,1][route],stats)
        hud.execute_ui_action("ArenaViewB")
        # Slate callbacks may run before the game viewport repaints in a background editor.
        for _ in range(120):
            yield
            if all(hud.get_action_center("InteriorEntry"+letter).x>=0 for letter in "AB"): break
        check(label+" 선내 지휘 지도",prop(hud,"interior_ship")==target,prop(hud,"interior_ship"))
        for number,letter in enumerate("AB"):
            button="InteriorEntry"+letter
            point=hud.get_action_center(button)
            check(label+" 지도 버튼 "+letter,str(hud.get_hit_action_at_canvas_point(point))==button,str(point))
            hud.handle_interior_action(button)
            decks={prop(e,"deck") for e in entries if prop(transit.get_person(prop(e,"person_id")),"boarding_team_id")==prop(transit.get_person(people[number*12]),"boarding_team_id")}
            check(label+" 실제 진입층 "+letter,prop(hud,"interior_deck") in decks,decks)
        hud.handle_interior_action("InteriorClose")
        before_recall={i for i in people if prop(transit.get_person(i),"state")==u.OVPassengerState.DEAD}
        pc.action("ArenaReturnA")
        pc.action("ArenaReturnB")
        for _ in range(2400):
            pc.step_simulation(.05)
            if prop(pc.get_play_scenario(),"result")!=u.OVPlaySceneResult.RUNNING: break
        stats["복귀 결과"]=str(prop(pc.get_play_scenario(),"result"))
        stats["귀환 강습선"]=len(docked())
        at_home=set(interior.get_units_on_ship(home,0))
        stats["실제 귀환 인원"]=len(at_home.intersection(ids))
        stats["사망"]=sum(prop(transit.get_person(i),"state")==u.OVPassengerState.DEAD for i in people)
        check(label+" 생존자 원함 귀환",stats["실제 귀환 인원"]>0 and stats["실제 귀환 인원"]+stats["사망"]==24 and set(docked())==originals and prop(pc.get_play_scenario(),"result")==u.OVPlaySceneResult.PARTIAL_VICTORY,stats)
        losses=[[0,0] for _ in range(3)]
        by_team={}
        for i in people:
            person=transit.get_person(i)
            group=prop(person,"boarding_team_id")
            team=by_team.setdefault(group,len(by_team))
            stage=int(prop(person,"boarding_phase").value)
            if prop(person,"state")==u.OVPassengerState.DEAD:losses[stage][team]+=1
        check(label+" 철수 전후 손실 대조",sum(losses[0])==0 and sum(losses[1])==len(before_recall) and sum(losses[2])==stats["사망"]-len(before_recall),[len(before_recall),losses])
        lines=list(prop(pc.get_play_scenario(),"debrief_lines"))
        for phase,title in enumerate(["진입 전","선내 교전","철수 중"]):
            row=next((str(line) for line in lines if str(line).startswith(title)), "")
            expected=f"A팀  전사 {losses[phase][0]} · 구조 대기 0      B팀  전사 {losses[phase][1]} · 구조 대기 0"
            check(label+" 결산 "+title,expected in row,row)
        fallen={prop(transit.get_person(i),"runtime_unit_id") for i in people if prop(transit.get_person(i),"state")==u.OVPassengerState.DEAD}
        personal=[n for n in prop(feed,"notices") if prop(n,"kind") in (u.OVBattleNoticeKind.TROOPER_DEATH,u.OVBattleNoticeKind.HERO_DEATH)]
        check(label+" 전사 개인 단위 기록",fallen=={prop(n,"unit_id") for n in personal} and len(personal)==len(fallen),sorted(fallen))
        stats["단계별 전사 A팀·B팀"]=losses
        yield
    assert pc.start_play_scenario("HeroArena")
    pc.action("ArenaRoute")
    pc.action("ArenaQuota")
    pc.action("ArenaFight")
    shuttle=prop(transit,"shuttles")[0]
    people=list(prop(shuttle,"passenger_ids"))
    check("영웅 없는 일반병 강습선 출발",len(set(people))==12 and all(not prop(transit.get_person(i),"runtime_hero") for i in people),len(people))
    result["passed"]=True

def tick(_):
    try:
        next(job)
        return
    except StopIteration: pass
    except Exception: result["error"]=traceback.format_exc()
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()
job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
