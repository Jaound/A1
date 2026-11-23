"""영웅 시험장의 실제 편성·포드·함교 교전을 검사한다."""
import json
import traceback
from pathlib import Path
import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
out=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/"Validation/HeroArena.json"
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
    assert pc.start_play_scenario("HeroArena")
    ships=list(p(pc,"ships"))
    interior=p(pc,"interior")
    def units(): return list(p(interior,"units"))
    check("구축전함 두 척",len(ships)==2 and all("구축전함" in p(x,"ship_name") for x in ships),len(ships))
    for x in ships:
        systems=p(x,"systems")
        check(str(x.get_name())+" 무장·쉴드 해제",len(p(p(systems,"layout"),"weapon_mounts"))==0 and p(systems,"max_shield")==0, p(systems,"max_shield"))
    check("편성 전 적 수호대",len(units())==10,len(units()))
    pc.action("ArenaNext")
    pc.action("ArenaAdd")
    pc.action("ArenaFill")
    pc.action("ArenaFill")
    check("양측 10명과 고유 개인 ID",len(units())==20 and len({str(p(x,"hero_id")) for x in units()})==20,len(units()))
    pc.action("ArenaFight")
    check("원 포드 10기",len(p(p(pc,"transport"),"pods"))==10,len(p(p(pc,"transport"),"pods")))
    bridge=next(p(x,"slot_id") for x in p(p(p(ships[1],"systems"),"layout"),"slots") if p(x,"facility")==u.OVFacilityType.BRIDGE)
    ordered=set()
    for i in range(2400):
        pc.step_simulation(.05)
        if i%5: continue
        for x in units():
            uid=p(x,"unit_id")
            if uid not in ordered and p(x,"faction_id")==0 and p(x,"ship")==ships[1]:
                if interior.issue_order([uid],u.OVInteriorOrder.CAPTURE,bridge): ordered.add(uid)
        if len(ordered)==10 and sum(p(x,"hits") for x in units())>0:break
    check("열 명 실제 진입 및 이동 명령",len(ordered)==10,len(ordered))
    hits=sum(p(x,"hits") for x in units())
    check("선내 실제 명중",hits>0,hits)
    assert pc.start_play_scenario("Fleet50")
    check("일반 함대 무장 복원",any(len(p(p(p(x,"systems"),"layout"),"weapon_mounts")) for x in p(pc,"ships")),len(p(pc,"ships")))
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
