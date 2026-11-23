"""선내 지도 카드 필터·슬라이더·개별 선택을 실제 시험장에서 검증한다."""
import json
import traceback
import unreal as u
from pathlib import Path
folder=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/"Validation/InteriorCards"
folder.mkdir(parents=True,exist_ok=True)
results={}
def check(name,ok):
    results[name]=bool(ok)
    assert ok,name
ed=u.get_editor_subsystem(u.LevelEditorSubsystem)
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
def p(x,k): return x.get_editor_property(k)
def run():
    while not editor.get_game_world(): yield
    w=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(w,0)
    while not pc or not pc.get_hud():
        yield
        pc=u.GameplayStatics.get_player_controller(w,0)
    assert pc.start_play_scenario("HeroArena")
    hud=pc.get_hud()
    hud.close_title_screen()
    pc.action("ArenaAdd")
    pc.action("ArenaFill")
    interior=p(pc,"interior")
    home=p(pc,"ships")[0]
    hud.handle_interior_action("InteriorToggle")
    hud.handle_interior_action("InteriorSelectedShip")
    def draw():
        for _ in range(4): yield
    def cards():
        return {p(unit,"unit_id") for unit in p(interior,"units") if hud.get_action_center("InteriorCard_"+str(p(unit,"unit_id"))).x>=0}
    def picture(name):
        u.SystemLibrary.execute_console_command(w,'Shot SHOWUI filename="'+(folder/(name+".png")).as_posix()+'" -nosuffix',pc)
        for _ in range(12):yield
    yield from draw()
    original=cards()
    width,height=pc.get_viewport_size()
    scale=max(.25,min(4,min(width/1280,height/740)*hud.get_user_ui_scale_multiplier()))
    left=hud.get_action_center("InteriorPrev")
    right=hud.get_action_center("InteriorNext")
    check("가로 이동 양쪽 버튼",right.x>left.x>0)
    # Move the thumb using the same press/drag/release path as the controller.
    def pointer(x,down,press=False):
        return hud.handle_pointer(x*scale,left.y*scale,press,down,0,False,0,0)
    check("슬라이더 누름 소비",pointer(left.x+32,True,True))
    pointer(right.x-27,True)
    pointer(right.x-27,False)
    yield from draw()
    check("가로 드래그로 끝 인원 노출",bool(cards()) and cards().isdisjoint(original))
    at_end=cards()
    hud.handle_pointer((right.x-50)*scale,(left.y-32)*scale,False,False,1,False,0,0)
    yield from draw()
    check("카드 위 휠로 한 열 이동",cards()!=at_end)
    for idx,expected in [(1,12),(2,12),(3,10),(0,34)]:
        hud.execute_ui_action("InteriorFilter_"+str(idx))
        yield from draw()
        seen=cards()
        first=seen.copy()
        for _ in range(20):
            hud.handle_interior_action("InteriorNext")
            yield from draw()
            seen.update(cards())
        check("필터별 전체 명단 "+str(idx),len(seen)==expected)
        if idx==3:reserve=seen
        if idx==1:team_a=seen
        if idx==2:check("A·B팀 배타성",seen.isdisjoint(team_a))
    hud.execute_ui_action("InteriorFilter_1")
    yield from draw()
    check("예비와 직접 편성 분리",reserve.isdisjoint(team_a))
    soldiers=sorted(p(unit,"unit_id") for unit in p(interior,"units") if p(unit,"ship")==home and p(unit,"role")==u.OVInteriorRole.TROOPER)
    first_id=min(cards())
    hud.execute_ui_action("InteriorCard_"+str(first_id))
    check("개인 카드 선택",list(p(hud,"interior_selected_units"))==[first_id])
    yield from draw()
    yield from picture("Home")
    hud.execute_ui_action("InteriorFilter_2")
    check("필터 밖 인원에 명령 방지",not p(hud,"interior_selected_units"))
    hud.handle_interior_action("InteriorClose")
    pc.action("ArenaFight")
    interior=p(pc,"interior")
    target=p(pc,"ships")[1]
    layout=p(p(target,"systems"),"layout")
    slots=p(layout,"slots")
    bridge=next(p(slot,"slot_id") for slot in slots if p(slot,"facility")==u.OVFacilityType.BRIDGE)
    ordered=set()
    for i in range(2400):
        pc.step_simulation(.05)
        if i%5:continue
        boarders=[x for x in p(interior,"units") if p(x,"faction_id")==0 and p(x,"ship")==target]
        for x in boarders:
            uid=p(x,"unit_id")
            if uid not in ordered and interior.issue_order([uid],u.OVInteriorOrder.CAPTURE,bridge):ordered.add(uid)
        iris=next((x for x in boarders if str(p(x,"definition_id"))=="M02"),None)
        if iris and len(ordered)==12 and sum(p(x,"hits") for x in boarders)>0:break
    assert iris and len(ordered)==12
    hud.execute_ui_action("ArenaViewA")
    deck=next(p(slot,"deck_id") for slot in slots if p(slot,"slot_id")==p(iris,"node_id"))
    hud.handle_interior_action("InteriorDeck_"+str(deck))
    for _ in range(8): yield
    def squad_markers():
        return [p(unit,"unit_id") for unit in boarders if hud.get_action_center("InteriorSquad_"+str(p(unit,"unit_id"))).x>=0]
    heads=squad_markers()
    check("실제 진입 후 분대 표식",len(heads)>0)
    hud.execute_ui_action("InteriorSquad_"+str(heads[0]))
    leader=interior.get_unit(heads[0])
    expected={p(unit,"unit_id") for unit in boarders if p(unit,"node_id")==p(leader,"node_id") and p(unit,"unit_id") in soldiers[(soldiers.index(heads[0])//4)*4:(soldiers.index(heads[0])//4+1)*4] and p(unit,"hp")>0}
    check("현재 방의 생존 분대원만 선택",set(p(hud,"interior_selected_units"))==expected)
    card=next((n for n in cards() if n in expected),None)
    if card is None:
        hud.handle_interior_action("InteriorNext")
        yield from draw()
        card=next(n for n in cards() if n in expected)
    hud.execute_ui_action("InteriorCard_"+str(card))
    check("분대에서 개인 한 명 지휘",list(p(hud,"interior_selected_units"))==[card])
    hud.execute_ui_action("InteriorFilter_1")
    yield from draw()
    iris_card="InteriorCard_"+str(p(iris,"unit_id"))
    check("교전 중 영웅 카드 클릭 영역",str(hud.get_hit_action_at_canvas_point(hud.get_action_center(iris_card)))==iris_card)
    hud.execute_ui_action(iris_card)
    yield from draw()
    yield from picture("Boarding")
    hud.handle_interior_action("InteriorMax")
    yield from draw()
    yield from picture("Expanded")
    old_scale=hud.get_user_ui_scale_multiplier()
    hud.set_user_ui_scale_multiplier(1.25,False)
    yield from draw()
    check("UI 125% 영웅 카드 입력 영역",str(hud.get_hit_action_at_canvas_point(hud.get_action_center(iris_card)))==iris_card)
    hud.set_user_ui_scale_multiplier(old_scale,False)
    results["passed"]=True
def tick(_):
    try:
        next(job)
        return
    except StopIteration:pass
    except Exception:results["error"]=traceback.format_exc()
    (folder/"Result.json").write_text(json.dumps(results,indent=2,ensure_ascii=False),encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()
job=run()
ed.editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
