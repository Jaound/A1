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
    def reach(label,limit=1400):
        for _ in range(limit):
            play.observe_practice()
            if play.practice_ready:break
            pc.step_simulation(.25)
        advance(label)
    def slot(ship,kind):
        return next(s.slot_id for s in ship.systems.layout.slots if s.facility==kind)
    def room(ship,kind):
        hud.execute_ui_action("InteriorSlot_"+str(slot(ship,kind)))
    ships=begin(1);ship=ships[0]
    pc.select_ship(ship);advance("항모 선택")
    hud.set_details_open(True);advance("선체 상세")
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
    ship,main=begin(4)
    pc.select_ship(ship);advance("방공함 선택")
    ship.set_destination(main.get_actor_location()+u.Vector(10500,0,0));reach("호위 표식 도착")
    ship=begin(5)[0];hud.execute_ui_action("InteriorToggle")
    for kind in [u.OVFacilityType.CORE,u.OVFacilityType.BRIDGE,u.OVFacilityType.ENGINE,u.OVFacilityType.WEAPON_CONTROL]:
        room(ship,kind);advance("시설 확인 "+str(kind))
    for label in ("호환 무장 실제 장착","상위 티어 장착 거부"):
        hud.execute_ui_action("PracticeEquip");advance(label)
    ship=begin(7)[0];hud.execute_ui_action("HeroPanel");advance("영웅 명단")
    interior=pc.interior
    hero=next(a for a in interior.units if a.faction_id==0 and a.role!=u.OVInteriorRole.TROOPER)
    duty=slot(ship,u.OVFacilityType.WEAPON_CONTROL if hero.role==u.OVInteriorRole.RANGED else u.OVFacilityType.BRIDGE)
    check("지원 보직 지시",interior.set_hero_duty(hero.unit_id,True,duty));reach("영웅 보직 활성")
    check("수호로 변경",interior.set_hero_duty(hero.unit_id,False,duty));advance("지원 효과 해제")
    check("지원 복귀",interior.set_hero_duty(hero.unit_id,True,duty));reach("동일 영웅 복귀")
    home,target,*_=begin(8)
    troops=[a for a in pc.interior.units if a.ship==home and a.faction_id==0]
    ids=[a.unit_id for a in troops if a.role!=u.OVInteriorRole.TROOPER][:4]+[a.unit_id for a in troops if a.role==u.OVInteriorRole.TROOPER][:8]
    check("실제 강습 편성 및 적함 지정",len(ids)==12 and pc.prepare_boarding(home,target,None,ids))
    advance("강습 목표 접수")
    for _ in range(1200):
        pc.step_simulation(.05)
        if pc.confirm_boarding(pc.last_crew_operation()):break
    reach("탑승 후 포드 발사");reach("적함에 실제 진입")
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
    reach("원 수송수단으로 회수",2400)
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
