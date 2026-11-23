"""별도 PIE에서 커서 배치, 전투 등록과 중단·재개를 검사한다."""
import json
from pathlib import Path
import time
import traceback
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / time.strftime("SandboxFleet_%Y%m%d_%H%M%S")
out.mkdir(parents=True)
report = {"checks": [], "physical_input": False}
def p(obj, name):
    return obj.get_editor_property(name)
def check(name, ok):
    if not ok:
        raise AssertionError(name)
    report["checks"].append(name)

def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    while not pc or not pc.get_hud():
        yield
        pc = u.GameplayStatics.get_player_controller(world, 0)
    play = pc.get_play_scenario()
    combat = p(pc, "combat")
    carrier=p(pc,"carrier")
    pd=p(pc,"point_defense")
    def ghosts():
        return list(u.GameplayStatics.get_all_actors_with_tag(world,"SandboxHullPreview"))
    def registered():
        return list(p(pc,"ships"))
    def advance(seconds):
        for _ in range(4*seconds): pc.step_simulation(.25)
    def xyz(v):
        return (v.x,v.y,v.z)
    def snapshot():
        return [p(combat,"shot_count"),p(combat,"hit_count"),p(combat,"game_time"),
                [(xyz(x.get_actor_location()),p(p(x,"systems"),"hull_hp"),p(p(x,"systems"),"shield")) for x in registered()],
                [(p(a,"aircraft_id"),p(a,"hp"),str(p(a,"state")),xyz(p(a,"position"))) for a in p(carrier,"aircraft")],
                [(p(a,"attack_id"),xyz(p(a,"position"))) for a in p(combat,"attacks")]]
    at=lambda x,y=0,z=2000: u.Vector(x,y,z)
    act=pc.action
    assert pc.start_play_scenario("Sandbox")
    check("빈 전장·일시정지",not registered() and not p(combat,"ships") and p(pc,"simulation_paused"))
    for action in ("Ship1","Ship2","Ship3","SBStart"): act(action)
    check("빈 전장 교전 시작 차단",not p(play,"sandbox_engaged") and p(pc,"simulation_paused"))
    act("SBPlace")
    check("미리보기만으로 전력 증가 없음",len(ghosts())==1 and not registered() and not p(combat,"ships"))
    act("SBCount")
    check("세 척 편대 미리보기",len(ghosts())==3)
    check("편대 끝이 경계를 넘으면 전체 배치 차단",not play.place_sandbox_ship(at(-12000,180000)) and not registered())
    check("아군 3척 동시 배치",play.place_sandbox_ship(at(-14000)) and len(registered())==3)
    group=registered()
    check("아군 위치·간격·방향",all(abs(x.get_actor_location().x+14000)<1 and p(x,"friendly") for x in group)
          and group[0].get_actor_location().y<0<group[-1].get_actor_location().y and abs(group[1].get_actor_location().y)<1)
    check("겹친 재배치 차단",not play.place_sandbox_ship(at(-14000)) and len(registered())==3)
    act("SBStart")
    check("한쪽만 배치하면 준비 유지",not p(play,"sandbox_engaged") and not p(combat,"shot_count"))
    act("SBCount"); act("SBCount")
    act("SBMenu"); act("SBHull_carrier"); act("SBEnemy"); act("SBPlace")
    check("실제 항모 메시 미리보기",len(ghosts())==1 and 'CarrierFull' in ghosts()[0].static_mesh_component.static_mesh.get_path_name())
    check("적군 항모 실제 등록",play.place_sandbox_ship(at(16000,0,7000)))
    ship=registered()[-1]
    check("진영과 높이·방향 적용",not p(ship,"friendly") and ship.get_actor_location().z==7000 and ship.get_actor_forward_vector().x<-.99)
    check("교전·PD·실제 격납 재고 등록",len(p(combat,"ships"))==4 and len(p(pd,"ships"))==4 and len(p(carrier,"aircraft"))==20)
    act("Cancel")
    check("미리보기 취소 후 배치 차단",not ghosts() and not play.place_sandbox_ship(at(35000)))
    act("SBPlace")
    act("SBStart")
    check("교전 시작 시 미리보기 해제",p(play,"sandbox_engaged") and not p(pc,"simulation_paused") and not ghosts())
    act("SBPlace")
    check("교전 중 생성 금지",not ghosts() and not play.place_sandbox_ship(at(40000)))
    check("양측 접근 목적지 간격 유지",(p(group[1],"destination")-p(ship,"destination")).length()>15000)
    advance(9)
    check("실제 발사와 명중",p(combat,"shot_count")>0 and p(combat,"hit_count")>0)
    act("SBStop")
    before=snapshot()
    for speed in (1,2,3):
        act("Speed"+str(speed)); advance(2)
        check(f"교전 중단 {speed}배속 전력·피해·위치·시간 보존",before==snapshot())
    act("SBStart"); advance(2)
    check("기존 개체 그대로 교전 재개",registered()==group+[ship] and p(combat,"game_time")>before[2] and p(combat,"shot_count")>before[0])
    act("Pause")
    check("단순 일시정지는 전투 모드 유지",p(play,"sandbox_engaged") and p(pc,"simulation_paused"))
    act("SBStop")
    pc.select_ship(group[0]); act("SBRemove")
    check("선택 함선과 전투 등록 제거",group[0] not in registered() and len(p(combat,"ships"))==3)
    act("RestartScene")
    check("전체 초기화·재고 정리",not registered() and not p(carrier,"aircraft") and not p(pd,"ships") and not p(play,"sandbox_engaged"))
    hulls="corvette frigate vector bastion cruiser air_defense radar battleship carrier missile_ship dreadnought battlecarrier repair mining transport assault".split()
    for hull in hulls:
        act("SBMenu"); act("SBHull_"+hull); act("SBPlace")
        check(hull+" 미리보기·생성",len(ghosts())==1 and play.place_sandbox_ship(at(10000)))
        pc.select_ship(registered()[0]); act("SBRemove")
        check(hull+" 제거·전력 정리",not registered() and not p(combat,"ships") and not p(carrier,"aircraft"))
        act("SBPlace")
    act("SBPlace")
    assert pc.start_play_scenario("Fleet100")
    check("장면 전환 시 미리보기와 편집 권한 정리",not ghosts() and len(registered())==24 and not play.place_sandbox_ship(at(16000)))
    report["shots_at_stop"],report["hits_at_stop"],report["game_time_at_stop"]=before[:3]

def tick(delta):
    try:
        next(job)
        return
    except StopIteration:
        report["passed"]=True
    except Exception:
        report.update(passed=False,error=traceback.format_exc())
    (out/"result.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.OVDevelopmentEditorLibrary.restore_preview_settings()
    u.SystemLibrary.quit_editor()

job=run()
assert u.OVDevelopmentEditorLibrary.start_preview_window(1280,720)
handle=u.register_slate_post_tick_callback(tick)
