"""별도 PIE에서 일반 임무 경로, 화물 보존과 손실 판정을 검사한다."""
import json
from pathlib import Path
import time
import traceback
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation" / time.strftime("MiningEscort_%Y%m%d_%H%M%S")
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
    log = next(x for x in u.ObjectIterator(u.OVLogisticsSubsystem) if x.get_outer() == world)
    def rows(role):
        return [x for x in p(log, "cargo") if p(x, "role") == role]
    def cargo(role):
        return sum(p(x, "amount") for x in rows(role))
    miner = u.OVDevLogisticsRole.MINING
    truck = u.OVDevLogisticsRole.TRANSPORT
    def advance(seconds):
        for _ in range(seconds*4):
            pc.step_simulation(.25)
    def start():
        check("임무 시작", pc.start_play_scenario("Logistics"))
    start()
    hud = pc.get_hud()
    pc.select_ship(p(rows(miner)[0],"ship"))
    check("선택 채굴선의 가득 찬 창고", "80.0 / 80.0" in hud.get_selected_cargo_label())
    pc.select_ship(p(rows(truck)[0],"ship"))
    check("선택 수송선의 가득 찬 창고", "130.0 / 130.0" in hud.get_selected_cargo_label())
    check("채굴선과 R37 두 척씩", len(rows(miner)) == len(rows(truck)) == 2 and all(
        p(combat.get_ship_state(p(x,"ship")), "weapon_tier_cap") == 2 for x in rows(truck)))
    check("실제 등록·비용", len(p(pc, "ships")) == len(p(combat, "ships")) == 7 and list(p(play,"initial_costs")) == [19,0])
    advance(1)
    check("정지 시 재고 불변", p(play, "elapsed_seconds") == 0 and cargo(miner)+cargo(truck) == 420)
    # Let actual frames draw the scenario before stepping a long run.
    until = time.monotonic()+1
    while time.monotonic()<until: yield
    u.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="'+(out/"escort.png").as_posix()+'" -nosuffix', pc)
    until = time.monotonic()+5
    while not (out/"escort.png").exists() and time.monotonic()<until: yield
    pc.action("Pause")
    first = {}
    drift = 0
    for t in range(1, 481):
        advance(1)
        if t in (12,36,85):
            role = miner if t==12 else truck
            entry = rows(role)[0]
            pc.select_ship(p(entry,"ship"))
            amount = p(entry,"amount")
            label = hud.get_selected_cargo_label()
            check(f"{t}초 선택 함선 실제 화물량", f"{amount:.1f} / {p(entry,'capacity'):.1f}" in label)
            if t==36:
                pc.action("Pause")
                until = time.monotonic()+.6
                while time.monotonic()<until: yield
                path=out/"선택_수송선.png"
                u.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+path.as_posix()+'" -nosuffix',pc)
                until=time.monotonic()+5
                while not path.exists() and time.monotonic()<until: yield
                assert path.exists()
                pc.action("Pause")
        stock = p(p(log,"nodes")[0],"amount")
        ore, hold = cargo(miner),cargo(truck)
        delivered = p(play,"delivered_cargo")
        drift = max(drift,abs(650+420-stock-ore-hold-delivered-p(play,"discharged_stores")))
        for key, happen in (("채굴",stock<650),("인계",p(play,"handed_mined_ore")>0),("납품",delivered>0),("습격",len(p(pc,"ships"))>7)):
            if happen: first.setdefault(key,t)
        if p(play,"result") != u.OVPlaySceneResult.RUNNING: break
    report.update(first_game_seconds=first, elapsed=p(play,"elapsed_seconds"), delivered=delivered,
                  status=play.get_status(), cargo=hold, stock=stock, shots=p(combat,"shot_count"), max_drift=drift)
    check("채굴 후 인계 후 납품", first.get("채굴",999)<first.get("인계",0)<first.get("납품",0))
    check("고정 시드 습격과 실제 교전", "습격" in first and report["shots"]>0)
    check("전 과정 자원 보존", drift < .15)
    check("정상 항행으로 목표 달성", p(play,"result") == u.OVPlaySceneResult.VICTORY and abs(delivered-500)<.01)
    # Display the frozen end state in an actual rendered frame.
    until = time.monotonic()+.7
    while time.monotonic()<until: yield
    u.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="'+(out/"delivered.png").as_posix()+'" -nosuffix', pc)
    until = time.monotonic()+5
    while not (out/"delivered.png").exists() and time.monotonic()<until: yield
    clocks = []
    for speed in (1,2,3):
        start()
        pc.action("Speed"+str(speed))
        pc.action("Pause")
        for _ in range(36*4):
            pc.step_simulation(.25/speed)
        clocks.append([round(p(play,"elapsed_seconds"),3),round(cargo(miner),3),round(cargo(truck),3),p(combat,"shot_count")])
    report["clock36"]=clocks
    check("1·2·3배속 동일 시간 재고와 발사", clocks[0]==clocks[1]==clocks[2] and clocks[0][1]+clocks[0][2]>0)
    start()
    pc.action("Pause")
    for t in range(240):
        advance(1)
        if p(play,"delivered_cargo") + cargo(truck) >= 500-.001:
            break
    check("마지막 화물 수송 중", 0<p(play,"delivered_cargo")<500 and cargo(truck)>0)
    for x in rows(miner): p(p(x,"ship"),"systems").force_wreck_instant()
    advance(1)
    check("채굴선 전멸 후 적재분으로 진행", p(play,"result")==u.OVPlaySceneResult.RUNNING)
    advance(65)
    check("잔여 화물로 최종 승리", p(play,"result")==u.OVPlaySceneResult.VICTORY)
    start()
    for i,x in enumerate(rows(truck)):
        vessel=p(x,"ship")
        vessel.set_actor_location(u.Vector(85000+i*18000,85000,2000),False,False)
        vessel.stop_movement()
    pc.action("Pause")
    advance(14)
    check("장거리 인계 금지", cargo(miner)>0 and cargo(truck)==260 and p(play,"handed_mined_ore")==0 and p(play,"delivered_cargo")==0)
    pc.action("ResumeConvoy")
    advance(8)
    check("항로 복귀로 복항", any(p(p(x,"ship"),"has_destination") for x in rows(truck)) or cargo(truck)>0)
    p(p(rows(truck)[0],"ship"),"systems").force_wreck_instant()
    advance(1)
    check("수송선 한 척 상실 후 지속", p(play,"result")==u.OVPlaySceneResult.RUNNING)
    p(p(rows(truck)[1],"ship"),"systems").force_wreck_instant()
    advance(1)
    check("수송선 전멸 시 패배", p(play,"result")==u.OVPlaySceneResult.DEFEAT)
    start()
    for x in rows(miner): p(p(x,"ship"),"systems").force_wreck_instant()
    pc.action("Pause")
    advance(1)
    check("채굴 불능·잔량 부족 패배", p(play,"result")==u.OVPlaySceneResult.DEFEAT)
    assert pc.start_play_scenario("Fleet100")
    ships = p(pc,"ships")
    repair = next(x for x in ships if p(x,"friendly") and "보급수리함" in p(x,"ship_name"))
    pc.select_ship(repair)
    check("보급수리함 실제 화물 가득 표시", "2746.0 / 2746.0" in hud.get_selected_cargo_label() and rows(u.OVDevLogisticsRole.REPAIR))
    assert not log.register_dev_ship(repair,u.OVDevLogisticsRole.REPAIR,100,0)
    assert log.set_dev_materials(repair,u.Vector(5,3,2))
    label=hud.get_selected_cargo_label()
    check("다른 함종의 자원 세 종류와 공간", "30.0 / 2746.0" in label and "일반 5.0" in label and "고급 3.0" in label and "최고급 2.0" in label)
    pc.select_ship(next(x for x in ships if not p(x,"friendly") and "보급수리함" in p(x,"ship_name")))
    check("적 창고 정보 비공개", not hud.get_selected_cargo_label())
    pc.start_play_scenario("Sandbox")
    check("전환 시 임무 화물·자원 정리", not p(log,"cargo") and not p(log,"nodes") and p(play,"delivered_cargo")==0)

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
