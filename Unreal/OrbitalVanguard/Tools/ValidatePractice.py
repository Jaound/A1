"""독립 실습의 실제 선택·이동·출격·귀환 상태와 장 전환을 PIE에서 확인한다."""
import json
import time
import traceback
from pathlib import Path

import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
saved=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))
output=saved/"Validation"/"Practice"
output.mkdir(parents=True,exist_ok=True)
report={"checks":[]}
deadline=time.monotonic()+90

def check(name,value):
    report["checks"].append({"name":name,"passed":bool(value)})
    if not value:
        raise AssertionError(name)

def ok(value):
    return value[0] if isinstance(value,tuple) else value

def run():
    world=editor.get_game_world()
    while not world or not u.GameplayStatics.get_player_controller(world,0):
        if time.monotonic()>deadline:
            raise RuntimeError("PIE start timed out")
        yield
        world=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(world,0)
    hud=pc.get_hud()
    play=pc.get_play_scenario()
    def capture(name):
        if "-OVPracticeScreens" not in u.SystemLibrary.get_command_line():
            return
        path=output/(name+".png")
        for _ in range(4):
            yield
        u.SystemLibrary.execute_console_command(world,f'Shot showui -nosuffix filename="{path.as_posix()}"')
        for _ in range(30):
            yield
            if path.is_file():
                break
    def begin(chapter):
        hud.open_title_screen()
        title=hud.get_title_screen()
        title.open_practice()
        title.select_practice(chapter)
        title.launch_practice()
        check(f"{chapter+1}장 타이틀에서 진입",play.practice_chapter==chapter and play.practice_step==0 and not hud.is_title_screen_open())
        return list(pc.get_editor_property("ships"))
    def advance(label):
        play.observe_practice()
        if not play.practice_ready:
            report["unmet_objective"]={"chapter":play.practice_chapter+1,"goal":str(play.practice_goal_id()),"command":pc.last_action}
        check(label,play.advance_practice())
    def time_steps(n):
        for i in range(n):
            pc.step_simulation(.25)
            play.observe_practice()
    for c in range(9):
        ships=begin(c)
        check(f"{c+1}장 독립 편성",bool(ships) and u.SystemLibrary.is_valid(ships[0]))
        hud.execute_ui_action("PracticeReset")
        check(f"{c+1}장 재시작",play.practice_chapter==c and play.practice_step==0 and pc.get_editor_property("ships")[0]!=ships[0])
        hud.execute_ui_action("PracticeIndex")
        check(f"{c+1}장 목록 복귀",play.practice_chapter==-1 and hud.is_title_screen_open())
    ships=begin(0)
    check("조작 전 다음 차단",not play.advance_practice())
    pc.apply_camera_input(u.Vector2D(),u.Vector(1,0,0),0,0,.1)
    hud.execute_ui_action("PracticeRetry")
    check("같은 단계 재시도는 카메라 기준을 갱신",play.practice_step==0 and not play.advance_practice())
    pc.apply_camera_input(u.Vector2D(),u.Vector(1,0,0),0,0,.1)
    advance("카메라 이동")
    pc.apply_camera_input(u.Vector2D(50,10),u.Vector(),0,0,.1)
    advance("카메라 회전")
    pc.apply_camera_input(u.Vector2D(),u.Vector(),0,1,.1)
    advance("카메라 줌")
    pc.select_ship(ships[0])
    advance("단일 선택")
    pc.select_additional(ships[1])
    advance("다중 선택")
    pc.select_ship(ships[0])
    for dz in (0,4000):
        ships[0].set_destination(ships[0].get_actor_location()+u.Vector(7000,4500,dz))
        play.observe_practice()
        check("이동 명령만으로 완료되지 않음",not play.practice_ready)
        time_steps(300)
        advance("실제 함선 목표 도착 "+str(dz))
    pc.action("Pause")
    advance("전술 정지")
    pc.action("Speed2")
    if pc.simulation_paused:pc.action("Pause")
    advance("2배속과 장 완료")
    def reach(label,limit=1400):
        for _ in range(limit):
            play.observe_practice()
            if play.practice_ready:break
            pc.step_simulation(.25)
        advance(label)
    ships=begin(6)
    pc.select_ship(ships[0]);hud.set_details_open(True);hud.execute_ui_action("HangarView")
    advance("실제 격납고 열림")
    wing=pc.get_editor_property("carrier")
    ids=[a.aircraft_id for a in wing.aircraft]
    check("실습 항모 재고",len(ids)>0)
    wing.launch(ships[0]);advance("실제 출격")
    time_steps(8)
    wing.recall(ships[0]);advance("귀환 명령")
    time_steps(800)
    advance("같은 ID 기체 실제 재격납")
    check("재고 보존",ids==[a.aircraft_id for a in wing.aircraft])
    target=next(t for t in pc.ships if not t.friendly)
    pc.select_ship(ships[0]);pc.set_target(target);pc.action("LaunchAircraft")
    reach("실제 편대 공격으로 손상 표적 파괴")
    check("제공전 명령 없이 완료 차단",not play.advance_practice())
    pc.action("WingAirCombat");reach("제공전으로 적 기체 실제 추적")
    stock=wing.get_aircraft_overview(ships[0]).replacement_kits
    pc.action("WingSupply");advance("수리함 재고에서 항공 보급")
    check("실제 보충 자원 증가",wing.get_aircraft_overview(ships[0]).replacement_kits>stock)
    reach("실제 생산 후 보관")
    check("새 기체 추가",len(wing.aircraft)>len(ids))
    pc.set_target(next(t for t in pc.ships if not t.friendly and t.hull_definition_id=="carrier"))
    pc.action("AutoWing");reach("실제 자동 일제 출격")
    def slot(ship,kind):
        return next(s.slot_id for s in ship.systems.layout.slots if s.facility==kind)
    def room(ship,kind):
        hud.execute_ui_action("InteriorSlot_"+str(slot(ship,kind)))
    ships=begin(1);ship=ships[0]
    pc.select_ship(ship);advance("항모 선택")
    hud.set_details_open(True)
    yield from capture("ship-gauges")
    advance("선체 상세")
    hud.execute_ui_action("InteriorToggle");room(ship,u.OVFacilityType.CORE);advance("선내 방 선택")
    hud.execute_ui_action("HeroPanel");advance("탑승 영웅 명단")
    hud.set_hero_panel_open(False);hud.set_details_open(True);hud.execute_ui_action("HangarView");advance("기체 현황")
    hud.execute_ui_action("BattleFeedToggle")
    notice=hud.get_battle_notice_feed().notices[-1]
    hud.execute_ui_action("BattleNotice_"+str(notice.serial));advance("항모 알림 확인")
    ship,enemy=begin(2)
    ship.set_destination(ship.get_actor_location()+u.Vector(5000,0,0));advance("이동 접수")
    ship.stop_movement();advance("정지")
    pc.set_target(enemy);advance("수동 표적 지정")
    check("발사 요청",pc.combat.request_fire(ship))
    reach("수동 실제 발사")
    ship.set_automation(False,True);reach("자동 사격 발사")
    ship.set_automation(False,False);pc.action("CancelAttack");advance("사격 중지")
    ship,enemy,*escort=begin(3)
    pc.set_target(enemy);advance("적함 탐지")
    reach("무장 발사 가능")
    ship.set_automation(False,True)
    reach("실제 쉴드 피격");reach("장갑 이후 선체 피격",4000);reach("요격으로 격추",3000)
    ship,main,scout,radar,carrier,flagship,enemy=begin(4)
    pc.select_ship(ship);advance("방공함 선택")
    ship.set_destination(main.get_actor_location()+u.Vector(10500,0,0));reach("호위 표식 도착")
    pc.select_ship(scout);scout.set_destination(u.Vector(12000,-9500,2000));reach("초계함 전방 배치")
    pc.select_ship(radar);pc.action("RadarSurvey");pc.action("WorldPick_"+str(list(pc.ships).index(enemy)))
    reach("레이더함 적 내부 정찰")
    pc.select_ship(main);pc.set_target(enemy);pc.combat.request_fire(main);reach("전함 함포 발사")
    pc.select_ship(carrier);pc.set_target(enemy);pc.action("LaunchAircraft");reach("항모 편대 공격")
    pc.select_ship(flagship);hud.set_details_open(True);advance("초대형 구축전함 상세")
    # Pilot cards remain available after the required objectives; they use the normal selection and command path.
    initial=list(pc.ships)
    hud.execute_ui_action("PracticeMore")
    piloted={}
    for _ in range(32):
        hud.execute_ui_action("PracticePilot")
        vessel=pc.selected
        hull=str(vessel.hull_definition_id)
        if hull in piloted:
            check("함종 카드 한 바퀴 후 같은 함선 재선택",piloted[hull]==vessel)
            break
        piloted[hull]=vessel
        if hull=="repair":
            yield from capture("pilot-card")
        before=len(pc.ships);hud.execute_ui_action("PracticePilot")
        check("함종 카드 재선택·중복 방지 "+hull,len(pc.ships)==before and pc.selected==vessel)
        hud.execute_ui_action("PracticeRole")
    check("전체 16개 함종 카드에서 직접 지휘",len(piloted)==16 and len(pc.ships)==len(piloted)+1)
    check("필수 편성 보존",all(t in list(pc.ships) for t in initial))
    extra=[s for s in pc.ships if s not in initial]
    def hull_radius(vessel):
        half=vessel.systems.layout.local_half_extent
        scale=vessel.get_actor_scale3d()
        return u.Vector(half.x*scale.x,half.y*scale.y,half.z*scale.z).length()
    min_gap=min((a.get_actor_location()-b.get_actor_location()).length()-hull_radius(a)-hull_radius(b)
                for a in extra for b in pc.ships if a!=b)
    report["optional_ship_minimum_surface_gap_cm"]=round(min_gap,1)
    check("모든 추가 실습 함선의 배치 여유",min_gap>=195)
    pc.select_ship(piloted["transport"])
    pc.selected.set_destination(pc.selected.get_actor_location()+u.Vector(0,3000,0))
    check("직접 조종한 수송선 이동 명령",pc.selected.has_destination)
    pc.action("Stop")
    check("직접 조종한 수송선 정지 명령",pc.selected.hold and not pc.selected.has_destination)
    hud.execute_ui_action("PracticeReset")
    check("장 재시작 후 추가 함선 정리",len(pc.ships)==len(initial) and all(not u.SystemLibrary.is_valid(s) for s in extra))
    hud.execute_ui_action("PracticeMore");hud.execute_ui_action("PracticeRole");hud.execute_ui_action("PracticePilot")
    guest=pc.selected
    hud.execute_ui_action("PracticeIndex")
    check("목록 복귀 후 카드 함선 정리",not u.SystemLibrary.is_valid(guest))
    ship,gunship,missileship,scout,beamship,enemy=begin(5);hud.execute_ui_action("InteriorToggle")
    yield from capture("facility-lesson")
    for kind in [getattr(u.OVFacilityType,key) for key in ("CORE","BRIDGE","ENGINE","WEAPON_CONTROL","LIFE_SUPPORT","AMMO","CAPACITOR","MISSILE_STORAGE","SHIELD_CONTROL","RADAR","INTERCEPTION","BOARDING","HANGAR","FIGHTER_CONTROL","RESOURCE_STORAGE","SECURITY")]:
        room(ship,kind);advance("시설 확인 "+str(kind))
    for vessel,weapon in [(gunship,"함포"),(beamship,"빔"),(missileship,"미사일")]:
        pc.select_ship(vessel);pc.set_target(enemy);vessel.set_automation(False,True)
        reach(weapon+" 계열의 실제 사격")
        vessel.set_automation(False,False)
    for label in ("호환 무장 실제 장착","상위 티어 장착 거부"):
        hud.execute_ui_action("PracticeEquip");advance(label)
    ship=begin(7)[0];hud.execute_ui_action("HeroPanel");advance("영웅 명단")
    interior=pc.interior
    hero=next(a for a in interior.units if a.faction_id==0 and a.role!=u.OVInteriorRole.TROOPER)
    hud.set_hero_panel_open(False)
    hud.execute_ui_action("InteriorToggle");hud.execute_ui_action("InteriorCard_"+str(hero.unit_id));advance("영웅과 탑승 함선을 선내 지도에서 확인")
    duty=slot(ship,u.OVFacilityType.WEAPON_CONTROL if hero.role==u.OVInteriorRole.RANGED else u.OVFacilityType.BRIDGE)
    check("시설 보직 지시",interior.set_hero_duty(hero.unit_id,True,duty));reach("영웅 보직 활성")
    check("수호로 변경",interior.issue_order([hero.unit_id],u.OVInteriorOrder.DEFEND,slot(ship,u.OVFacilityType.BRIDGE)));reach("함교 수호 위치에 도착하고 보직 효과 해제")
    check("보직 복귀",interior.set_hero_duty(hero.unit_id,True,duty));reach("동일 영웅 복귀")
    home,target,*_=begin(8)
    pc.select_ship(home);hud.execute_ui_action("InteriorToggle")
    hud.execute_ui_action("InteriorFilter_1");hud.execute_ui_action("InteriorAll")
    ids=list(hud.interior_selected_units)
    yield from capture("assault-team")
    bay=next(h for h in pc.carrier.hangars if h.carrier==home and h.stored_small_ship and h.stored_small_ship.ship_name=="A팀 강습선")
    shuttle=bay.stored_small_ship
    check("개인 포드 준비 가능",bool(ids) and pc.prepare_boarding(home,target,None,ids))
    check("개인 포드로 강습선 연습 통과 차단",not play.advance_practice())
    pc.cancel_crew_operation(pc.last_crew_operation())
    check("실제 A팀 필터·전체 선택·강습선 준비",pc.prepare_boarding(home,target,shuttle,ids))
    advance("강습 목표 접수")
    for _ in range(1200):
        pc.step_simulation(.05)
        if pc.confirm_boarding(pc.last_crew_operation()):break
    reach("A팀 탑승 후 강습선 출발");reach("적함에 실제 진입")
    objective=slot(target,u.OVFacilityType.BRIDGE)
    ordered=set()
    for _ in range(6000):
        pc.step_simulation(.05)
        if _%5:continue
        for uid in pc.interior.get_units_on_ship(target,0):
            if uid not in ordered and pc.interior.issue_order([uid],u.OVInteriorOrder.CAPTURE,objective):ordered.add(uid)
        play.observe_practice()
        if play.practice_ready:break
    report["강습 시설 상태"]=[(a.unit_id,str(a.state),str(a.reason),str(a.node_id)) for a in pc.interior.units if a.unit_id in ids]
    advance("실제 시설 점령")
    survivors=list(pc.interior.get_units_on_ship(target,0))
    check("생존자 철수 명령",survivors and pc.interior.issue_order(survivors,u.OVInteriorOrder.RETREAT,""))
    reach("생존 대원이 강습선에 승선",2400)
    for _ in range(2400):
        pc.step_simulation(.25)
        if any(x.ship==shuttle and x.state==u.OVShuttleState.READY for x in pc.transport.shuttles):break
    check("실습 자동 수납 금지",not any(h.stored_small_ship==shuttle for h in pc.carrier.hangars))
    check("아군 모함 수납 명령",pc.request_small_ship_dock(bay.hangar_id,shuttle))
    reach("직접 명령 후 강습선의 실제 재격납",2400)
    hud.execute_ui_action("PracticeIndex")
def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        report["passed"]=True
    except Exception:
        report["passed"]=False
        report["error"]=traceback.format_exc()
    finally:
        if "passed" in report:
            (output/"PIE.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
