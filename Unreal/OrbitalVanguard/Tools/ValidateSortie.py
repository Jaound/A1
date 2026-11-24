"""출격 편집과 실제 생성 결과, 환경, 재시작을 PIE에서 대조한다."""
import json
import time
import traceback
from pathlib import Path

import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
saved=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))
output=saved/"Validation"/"Sortie"
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
    for budget in (50,100,200,300):
        mission="Fleet"+str(budget)
        for env,kind in enumerate([u.OVSpaceEnvironmentType.NONE,u.OVSpaceEnvironmentType.BLACK_HOLE,u.OVSpaceEnvironmentType.ASTEROID_FIELD]):
            prefix=f"{budget}코스트 · {env+1}번 환경 "
            check(prefix+"일반 출격 창",hud.begin_fleet_preparation(mission,False) and hud.is_title_screen_open())
            check(prefix+"전투 격리",play.elapsed_seconds==0 and pc.simulation_paused and not pc.ships)
            title=hud.get_title_screen()
            def cmd(action,index=0):
                return play.sortie_command(action,index)
            title.sortie_click("Map",env);title.sortie_click("Next")
            check(prefix+"예산 추천",play.sortie_cost()==budget and play.sortie_stage==1)
            enemy_ids=[str(play.sortie_hull(i)) for i in range(play.sortie_fleet_count())]
            if budget==50 and env==0:
                catalog=[str(x) for x in play.sortie_catalog()]
                check("예산 초과 거부",not cmd("Add",catalog.index("dreadnought")))
                while play.sortie_fleet_count():cmd("Remove",0)
                check("빈 함대 출격 거부",not cmd("Next"))
                for name in ("battleship","vector","corvette"):check("혼성 편성 "+name,cmd("Add",catalog.index(name)))
            ids=[str(play.sortie_hull(i)) for i in range(play.sortie_fleet_count())]
            check(prefix+"설계 이동",cmd("Next") and play.sortie_stage==2)
            fitted=None;changed_rooms={}
            if budget==50 and env==0:
                cmd("Ship",2);check("잘못된 교환 거부",not cmd("Swap",0));cmd("Tab")
                rejected=any(not cmd("Equip",i) for i in range(len(pc.combat.settings.weapons)))
                check("불법 장착 거부",rejected);cmd("Defaults")
                cmd("Ship",1);proxy=play.inspect_sortie(1)
                slots=list(proxy.systems.layout.slots)
                moved=False
                for a,left in enumerate(slots):
                    cmd("Room",a)
                    for b in range(a+1,len(slots)):
                        if cmd("Swap",b):
                            changed_rooms={str(left.slot_id):slots[b].facility,str(slots[b].slot_id):left.facility};moved=True;break
                    if moved:break
                check("실제 시설 교환",moved)
                mounts=pc.combat.get_ship_state(proxy).mounts
                for j,m in enumerate(mounts):
                    cmd("Slot",j)
                    for k,item in enumerate(pc.combat.settings.weapons):
                        if item.profile_id!=m.profile.profile_id and cmd("Equip",k):
                            fitted=(str(m.mount_id),str(item.profile_id));break
                    if fitted:break
                check("호환 무장 교체",fitted is not None)
            check(prefix+"배치 이동",cmd("Next") and play.sortie_stage==3)
            cmd("Ship",0)
            check(prefix+"아군 영역 경계",not play.set_sortie_position(u.Vector(22000,0,0),0))
            check(prefix+"함체 겹침",not play.set_sortie_position(play.sortie_location(1),0))
            if env==1:check(prefix+"블랙홀 중심 거부",not play.set_sortie_position(u.Vector(0,0,2000),0) and "블랙홀" in str(play.sortie_feedback))
            if env==2:check(prefix+"실체 겹침 차단",not play.set_sortie_position(u.Vector(-17000,23000,420),0))
            cmd("Auto");locations=[play.sortie_location(i) for i in range(len(ids))]
            check(prefix+"영웅 선택 이동",cmd("Next") and play.sortie_stage==4)
            check(prefix+"영웅 선택",cmd("Hero",0))
            check(prefix+"영웅 배치 이동",cmd("Next") and not cmd("Next"))
            cmd("Ship",0);cmd("Room",0)
            check(prefix+"영웅 시작 시설",cmd("Embark") and cmd("Next"))
            check(prefix+"최종 브리핑",play.sortie_stage==6 and play.elapsed_seconds==0)
            if budget==50 and env==0:
                check("최종에서 편성 수정",cmd("Review",1) and cmd("Remove",0))
                ids.pop(0);locations.pop(0)
                for _ in range(4):check("수정 후 다음 단계",cmd("Next"))
                check("삭제된 함선의 영웅 재배치 요구",not cmd("Next"))
                cmd("Ship",0);cmd("Room",0);check("남은 함선에 재배치",cmd("Embark") and cmd("Next"))
            if budget==50 and env==0:
                for name,phase in (("briefing",6),("hero-placement",5)):
                    if phase!=6:title.sortie_click("Review",phase)
                    else:title.sortie_click("Detail")  # synchronize view after direct subsystem editing
                    for _ in range(8):yield
                    file=output/(name+".png")
                    u.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+file.as_posix()+'" -nosuffix',pc)
                    until=time.monotonic()+4
                    while not file.exists() and time.monotonic()<until:yield
                title.sortie_click("Next")
            title.sortie_click("Start")
            check(prefix+"출격",str(play.active_id)==mission and not hud.is_title_screen_open())
            allies=[s for s in pc.ships if s.systems.faction_id==0]
            enemies=[s for s in pc.ships if s.systems.faction_id==1]
            check(prefix+"실제 아군 함종",[str(s.hull_definition_id) for s in allies]==ids)
            check(prefix+"실제 적 공개 편성",[str(s.hull_definition_id) for s in enemies]==enemy_ids)
            check(prefix+"위치와 고도",all((s.get_actor_location()-pt).length()<1 for s,pt in zip(allies,locations)))
            check(prefix+"실제 탑승 영웅",any(unit.ship==allies[0] and unit.role!=u.OVInteriorRole.TROOPER for unit in pc.interior.units))
            if changed_rooms:
                check("실제 함선의 시설 배치",all(any(str(s.slot_id)==key and s.facility==value for s in allies[0].systems.layout.slots) for key,value in changed_rooms.items()))
                check("실제 함선의 무장",any((str(m.mount_id),str(m.profile.profile_id))==fitted for m in pc.combat.get_ship_state(allies[0]).mounts))
            check(prefix+"환경 일치",pc.active_environment==kind)
            check(prefix+"중복 출격 차단",not play.commit_sortie())
            if env==2:check(prefix+"소행성 파괴",pc.damage_environment_asteroid(0,100000))
            if budget==50 and env==0:
                pc.action("RestartScene")
                check("같은 편성 재시작",[str(s.hull_definition_id) for s in pc.ships if s.systems.faction_id==0]==ids)
            pc.leave_play_scenario()
            check(prefix+"환경 정리",pc.active_environment==u.OVSpaceEnvironmentType.NONE)
            yield
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
