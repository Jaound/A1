"""300코스트전의 실제 함선·시설·대함 미사일로 공유 탐지와 물리 사거리를 분리해 검사한다."""
import json
import traceback
from pathlib import Path
import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
out=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/"Validation/RadarSupport.json"
result={"checks":{}}
def p(o,k):
    return o.get_editor_property(k)
def check(name, condition, value):
    result["checks"][name]={"passed":bool(condition),"value":str(value)}
    assert condition, (name,value)

def run():
    while not editor.get_game_world(): yield
    w=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(w,0)
    while not pc or not pc.get_hud():
        yield
        pc=u.GameplayStatics.get_player_controller(w,0)
    assert pc.start_play_scenario("Fleet300")
    combat=p(pc,"combat")
    ships=list(p(pc,"ships"))
    def find(label,ally):
        return next(ship for ship in ships if label in p(ship,"ship_name") and p(ship,"friendly")==ally)
    source,radar,target=find("전략",True),find("레이더",True),find("초계",False)
    for i,ship in enumerate(ships):
        ship.set_automation(False,False)
        ship.stop_movement()
        ship.set_actor_location(u.Vector(i*50000,1500000,0),False,False)
    source.set_actor_location(u.Vector(0,0,0),False,False)
    source.set_actor_rotation(u.Rotator(0,0,0),False)
    target.set_actor_location(u.Vector(154000,0,0),False,False)
    sys=p(radar,"systems")
    check("레이더함 자체 범위(m)",sys.get_radar_range()==145000,sys.get_radar_range()/100)
    mid=p(p(combat.get_ship_state(source),"mounts")[0],"mount_id")
    assert combat.set_target(source,target)
    def reason(): return p(combat.get_readiness(source,mid),"reason")
    check("1540m 지원선 부재",reason()=="NO DETECTION",reason())
    radar.set_actor_location(u.Vector(35000,20000,0),False,False)
    detection=combat.get_detection(source,target)
    check("1540m 전진 레이더함 탐지",p(detection,"source")=="SUPPORT RADAR",p(detection,"source"))
    pc.action("Pause")
    assert combat.request_fire(source,mid)
    for _ in range(160):
        pc.step_simulation(.05)
        if any(p(a,"attacker")==source and p(a,"anti_ship_missile") for a in p(combat,"attacks")): break
    check("공유 탐지로 실제 대함 미사일 발사",any(p(a,"attacker")==source and p(a,"anti_ship_missile") for a in p(combat,"attacks")),p(combat,"shot_count"))
    target.set_actor_location(u.Vector(170000,0,0),False,False)
    check("1700m 공유 탐지 유지",p(combat.get_detection(source,target),"detected"),p(combat.get_detection(source,target),"source"))
    check("1700m 무장 최대거리로 발사 차단",reason()=="OUT OF RANGE",reason())
    target.set_actor_location(u.Vector(154000,0,0),False,False)
    slots=p(p(sys,"layout"),"slots")
    for slot in slots:
        if p(slot,"facility")==u.OVFacilityType.RADAR:
            sys.debug_destroy_slot(p(slot,"slot_id"),u.OVDamageCause.SABOTAGE)
    check("레이더 시설 파괴 후 지원 소실",reason()=="NO DETECTION" and sys.get_radar_range()==0,reason())
    result["passed"]=True
def tick(_):
    try:
        next(job)
        return
    except StopIteration: pass
    except Exception: result["error"]=traceback.format_exc()
    out.parent.mkdir(exist_ok=True,parents=True)
    out.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()
job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
