"""영웅 시험장의 실제 편성·포드·함교 교전을 검사한다."""
import json
import traceback
import time
import statistics
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
    heroes=p(pc,"heroes")
    definitions=list(p(heroes,"definitions"))
    ids=[str(p(d,"id")) for d in definitions]
    check("34종 고유 정의",len(ids)==len(set(ids))==34,ids)
    owned_before=[str(p(h,"hero_id")) for h in heroes.get_heroes(0)]
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
    pc.action("ArenaPrev")
    pc.action("ArenaAdd")
    pc.action("ArenaFill")
    pc.action("ArenaFill")
    check("양측 10명과 고유 개인 ID",len(units())==20 and len({str(p(x,"hero_id")) for x in units()})==20,len(units()))
    iris=next(x for x in units() if str(p(x,"definition_id"))=="M02")
    check("이리스 정의와 런타임 연결",p(iris,"name")=="이리스 베인",p(iris,"name"))
    for name in ["standing","portrait","sd"]:
        asset=u.load_asset("/Game/UI/Heroes/M02/T_"+name)
        check("이리스 "+name+" 텍스처",isinstance(asset,u.Texture2D),asset.get_name())
    hud=pc.get_hud()
    hud.close_title_screen()
    hud.set_hero_panel_open(True)
    for _ in range(12): yield
    panel=hud.get_hero_panel()
    panel.refresh_cards()
    check("명단 카드에서 이리스 선택",panel.select_hero(p(iris,"hero_id")),len(panel.get_card_views()))
    def capture(name):
        file=out.parent/(name+".png")
        u.SystemLibrary.execute_console_command(w,'Shot SHOWUI filename="'+file.as_posix()+'" -nosuffix',pc)
        for _ in range(12): yield
    yield from capture("HeroArenaRoster")
    hud.set_hero_panel_open(False)
    hud.handle_interior_action("InteriorToggle")
    hud.handle_interior_action("InteriorSelectedShip")
    node=next(x for x in p(p(p(ships[0],"systems"),"layout"),"slots") if p(x,"slot_id")==p(iris,"node_id"))
    hud.handle_interior_action("InteriorDeck_"+str(p(node,"deck_id")))
    yield from capture("HeroArenaDeck")
    hud.handle_interior_action("InteriorClose")
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
    for i in range(1200):
        pc.step_simulation(.05)
        if i%10:continue
        iris=next(x for x in units() if str(p(x,"definition_id"))=="M02")
        if p(iris,"signature_uses") and p(iris,"passive_hits"):break
    result["이리스 교전 관찰"]={k:str(p(iris,k)) for k in ["hp","shots","hits","node_id","state","reason","signature_uses","passive_hits"]}
    result["수호대 관찰"]=[{k:str(p(x,k)) for k in ["node_id","state","shots","hp"]} for x in units() if p(x,"faction_id")==1]
    result["이리스 고경감 적중"]=p(iris,"passive_hits")
    assert pc.start_play_scenario("Fleet50")
    check("일반 함대 무장 복원",any(len(p(p(p(x,"systems"),"layout"),"weapon_mounts")) for x in p(pc,"ships")),len(p(pc,"ships")))
    pc.leave_play_scenario()
    check("원 명단 복원",[str(p(h,"hero_id")) for h in heroes.get_heroes(0)]==owned_before,owned_before)
    batches=[]
    for offset in range(0,len(ids),10):
        assert pc.start_play_scenario("HeroArena")
        position=ids.index("M02")
        pc.action("ArenaLevel")
        pc.action("ArenaLevel")
        for index in range(offset,min(offset+10,len(ids))):
            while position!=index:
                pc.action("ArenaNext")
                position=(position+1)%len(ids)
            pc.action("ArenaAdd")
        us=[x for x in units() if p(x,"faction_id")==0]
        expected=ids[offset:offset+10]
        check("정의별 실제 편성 "+str(offset),[str(p(x,"definition_id")) for x in us]==expected and all(p(x,"hero_level")==10 for x in us),expected)
        if len(expected)<10:pc.action("ArenaFill")
        pc.action("ArenaFight")
        for _ in range(240):pc.step_simulation(.05)
        arrived=[str(p(x,"definition_id")) for x in units() if p(x,"faction_id")==0 and p(x,"ship")==p(pc,"ships")[1]]
        check("정의별 실제 포드 진입 "+str(offset),all(x in arrived for x in expected),arrived)
        batches.append(expected)
    frames=[]
    last=time.perf_counter()
    for _ in range(240):
        yield
        now=time.perf_counter()
        frames.append((now-last)*1000)
        last=now
    result["PIE 프레임 간격(ms)"]={"조건":"마지막 시험장, 함선 2척·초기 영웅 20명, DX11, Slate 후처리 콜백 240프레임",
        "평균":statistics.mean(frames),"p95":sorted(frames)[227],"최대":max(frames)}
    ship_pair=list(p(pc,"ships"))
    all_friendly=[p(x,"unit_id") for x in units() if p(x,"faction_id")==0 and p(x,"ship")==ship_pair[1]]
    check("실제 철수 명령",len(all_friendly)==10 and interior.issue_order(all_friendly,u.OVInteriorOrder.RETREAT,""),all_friendly)
    scenario=pc.get_play_scenario()
    for _ in range(1800):
        pc.step_simulation(.05)
        if p(scenario,"result")!=u.OVPlaySceneResult.RUNNING:break
    returned=[x for x in p(p(pc,"transport"),"pods") if p(x,"state")==u.OVPodFlightState.RECOVERED]
    check("개인 포드 전원 회수와 철수 결과",len(returned)==10 and p(scenario,"result")==u.OVPlaySceneResult.PARTIAL_VICTORY,len(returned))
    clocks=[]
    for speed in [1,2,3]:
        assert pc.start_play_scenario("HeroArena")
        pc.action("ArenaAdd")
        pc.action("ArenaFill")
        pc.action("ArenaFight")
        pc.action("Pause")
        clock0=p(interior,"game_time")
        pc.step_simulation(3)
        check("일시정지 선내시계 "+str(speed),p(interior,"game_time")==clock0,clock0)
        pc.action("Pause")
        pc.action("Speed"+str(speed))
        for _ in range(600//speed): pc.step_simulation(1/60)
        ship_pair=list(p(pc,"ships"))
        inside=[p(x,"unit_id") for x in units() if p(x,"faction_id")==0 and p(x,"ship")==ship_pair[1]]
        target_bridge=next(p(x,"slot_id") for x in p(p(p(ship_pair[1],"systems"),"layout"),"slots") if p(x,"facility")==u.OVFacilityType.BRIDGE)
        assert len(inside)==10 and interior.issue_order(inside,u.OVInteriorOrder.CAPTURE,target_bridge)
        for _ in range(1200//speed):pc.step_simulation(1/60)
        friendly=[x for x in units() if p(x,"faction_id")==0]
        clocks.append({"배속":speed,"시계":p(interior,"game_time")-clock0,"명중":sum(p(x,"hits") for x in friendly),
                       "생존":sum(p(x,"hp")>0 for x in friendly),"남은 HP":sum(p(x,"hp") for x in friendly)})
    result["동일 게임 시간"]=clocks
    for c in clocks:
        check("동일 시간 교전 "+str(c["배속"]),abs(c["시계"]-clocks[0]["시계"])<.06 and c["명중"]==clocks[0]["명중"] and c["생존"]==clocks[0]["생존"] and abs(c["남은 HP"]-clocks[0]["남은 HP"])<1, c)
    pc.leave_play_scenario()
    assert p(pc,"development").open_roster_room()
    own=heroes.create_defined_hero(0,"M02")
    name_of=lambda key: str(p(heroes.get_hero(key),"definition_id"))
    check("동일 세력 고유 중복 차단",name_of(own)=="M02" and name_of(heroes.create_defined_hero(0,"M02"))!="M02",str(own))
    check("반대 세력 독립 명단",name_of(heroes.create_defined_hero(1,"M02"))=="M02",1)
    check("시험 지휘 단계",heroes.set_trial_command_tier(0,1) and heroes.deployment_limit(0)==3,heroes.deployment_limit(0))
    generic=[heroes.create_defined_hero(0,"G01") for _ in range(4)]
    check("세력 배치 3명 경계",all(heroes.set_deployment(x,True) for x in generic[:3]) and not heroes.set_deployment(generic[3],True),heroes.count_deployed(0))
    import uuid
    slot="OVNamedHeroTest_"+uuid.uuid4().hex
    try:
        assert heroes.grant_dev_xp(own,500) and heroes.save_roster(slot,0)
        before=heroes.get_hero(own)
        assert heroes.grant_dev_xp(own,500) and heroes.load_roster(slot,0)
        after=heroes.get_hero(own)
        check("개인 ID·정의·성장·지휘 한도 복원",name_of(own)=="M02" and p(before,"xp")==p(after,"xp") and heroes.deployment_limit(0)==3,str(own))
    finally:
        u.GameplayStatics.delete_game_in_slot(slot,0)
    result["편성·진입 검증"]=batches
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
