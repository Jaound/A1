"""Exercise the actual BaseTrial actor in PIE. Human input and cooking are separate."""
import json
import time
import traceback
from pathlib import Path

import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
saved=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))
output=saved/"Validation"/"BaseTrial4"
output.mkdir(parents=True,exist_ok=True)
report={"checks":[]}
slot=saved/"BaseTrial"/"BaseTrial.ovbase"
protected={path:(path.read_bytes() if path.is_file() else None)
           for path in (slot,slot.with_suffix(".ovbase.bak"),slot.with_suffix(".ovbase.tmp"))}
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
    ship=lambda:pc.get_editor_property("ships")[0]
    base=lambda:u.GameplayStatics.get_all_actors_of_class(world,u.OVBasePresentation)[0]

    check("기지 시험장 시작",pc.start_play_scenario("BaseTrial"))
    station=base()
    def advance(seconds):
        pc.action("Speed1")
        if pc.get_editor_property("simulation_paused"):
            pc.action("Pause")
        for _ in range(int(seconds*4)):
            pc.step_simulation(.25)
        if not pc.get_editor_property("simulation_paused"):
            pc.action("Pause")

    def verify(name, result):
        report["last_status"]=station.operation_status()
        check(name,result)

    verify("시험 조선소·도크·연구소 배치",station.prepare_operations_trial())
    check("세 운영 시설 코어 연결",all(station.is_module_connected(i) for i in (2,4,6)))
    stock=station.common_stock()
    verify("연구 예약",station.queue_research(1))
    verify("동일 연구 이중 예약 차단",not station.queue_research(1) and station.common_stock()==stock-800)
    advance(40)
    check("진행 중 연구 저장·복원",ok(station.save_trial()) and ok(station.load_trial()))
    verify("복원된 연구 중복 청구 차단",not station.queue_research(1) and station.common_stock()==stock-800)
    advance(201)
    check("연구 효과로 코어 승급 가능",station.upgrade_module(0))
    advance(300)
    verify("코어 II와 연구소 II를 별도 검사",not station.queue_research(4))
    check("연구소 II 증설",station.upgrade_module(6))
    advance(61)
    verify("중형 함체 정상 연구",station.queue_research(4))
    advance(301)
    check("중형 함체 효과로 중형 조선소 해금",station.reserve_module("Corridor",u.IntVector(0,-3,0),3) and station.reserve_module("YardMedium",u.IntVector(-5,-7,0),0))
    funds=station.common_stock()
    check("시험 연구 즉시 해금",station.prepare_trial_technology(7) and station.common_stock()==funds)
    verify("초계함 주문",station.order_hull(2,"corvette"))
    guid=station.facility_orders(2)[0]
    verify("작업칸 이중 예약 차단",not station.order_hull(2,"corvette"))
    check("준공 전 함선 미생성",station.vessel_phase(guid)==0 and not station.find_vessel(guid))
    advance(15)
    paid=station.common_stock()
    check("건조 중 저장",ok(station.save_trial()))
    advance(5)
    check("건조 중 복원",ok(station.load_trial()) and station.common_stock()==paid)
    advance(32)
    vessel=station.find_vessel(guid)
    verify("같은 주문으로 한 척 준공",vessel and station.vessel_phase(guid)==1 and len(pc.get_editor_property("ships"))==2)
    verify("첫 출항",station.launch_vessel(guid))
    check("출항 중 중복 명령 차단",not station.launch_vessel(guid))
    advance(6)
    verify("전장에 원래 함선 출현",station.vessel_phase(guid)==3 and station.find_vessel(guid)==vessel)
    verify("표준 도크 복귀 명령",station.dock_vessel(4,vessel))
    verify("동일 함선 이중 접안 차단",not station.dock_vessel(4,vessel))
    for _ in range(120):
        advance(4)
        if station.vessel_phase(guid)==6:break
    report["approach"]={"phase":station.vessel_phase(guid),"berth_cm":[round(value,2) for value in vessel.get_actor_location().to_tuple()]}
    verify("전장에서 입구 접근 후 정박",station.vessel_phase(guid)==6)
    systems=vessel.get_editor_property("systems")
    full=systems.get_editor_property("max_hull_hp")
    sections=systems.get_editor_property("armor_sections")
    hp_before=sum(a.get_editor_property("hp") for a in sections)
    for a in sections:
        systems.debug_set_armor(a.get_editor_property("armor_section_id"),0)

    verify("손상 견적 확인 후 수리",station.service_vessel(guid,False))
    reserve=station.common_stock()
    advance(2)
    check("수리 중 저장",ok(station.save_trial()))
    check("수리 중 복원 및 이중 청구 차단",ok(station.load_trial()) and reserve==station.common_stock())
    advance(60)
    check("장갑 복구",abs(sum(a.get_editor_property("hp") for a in systems.get_editor_property("armor_sections"))-hp_before)<.1)
    quote=station.refit_offer(guid)
    verify("장비 교체 주문",station.service_vessel(guid,True))
    advance(16)
    check("교체 장비 적용",station.vessel_phase(guid)==6 and station.refit_offer(guid)!=quote)
    check("탈거 장비 기지에 귀속",station.stored_equipment_count()==1)
    verify("보관 장비 재장착 주문",station.service_vessel(guid,True))
    advance(16)
    check("기존 장비 회수·재장착으로 장비 수 보존",station.refit_offer(guid)==quote and station.stored_equipment_count()==1)
    def rooms():
        return [str(x.get_editor_property("facility")) for x in systems.get_editor_property("layout").get_editor_property("slots")]
    original=rooms()
    verify("시설 재배치 주문",station.refit_facilities(guid))
    check("개조 도중 조선소 중복 예약 차단",not station.order_hull(2,"corvette"))
    advance(16)
    check("시설 교환 실제 적용",rooms()!=original and sorted(rooms())==sorted(original))
    layout_after=rooms()
    check("시설 개조 저장·복원",ok(station.save_trial()) and ok(station.load_trial()) and rooms()==layout_after)
    check("정박함 재출항",station.launch_vessel(guid))
    advance(6)
    check("재출항 후 같은 현물·ID",station.find_vessel(guid)==vessel and station.vessel_phase(guid)==3 and len(pc.get_editor_property("ships"))==2)

    check("재출항한 함대 저장",ok(station.save_trial()))
    check("새 임무 인스턴스로 재시작",pc.start_play_scenario("BaseTrial"))
    station=base()
    verify("새 인스턴스에서 온라인 함선 복원",ok(station.load_trial()) and station.find_vessel(guid) and station.vessel_phase(guid)==3)
    check("재호출로 물량 증가 없음",ok(station.load_trial()) and len(pc.get_editor_property("ships"))==2)

    check("경계 검사 초기화",pc.start_play_scenario("BaseTrial"))
    station=base()
    verify("경계 검사 편성",station.prepare_operations_trial())
    check("중복 편성 차단",not station.prepare_operations_trial())
    scout=ship()
    scout.set_actor_location(u.Vector(0,-18000,0),False,True)
    scout.set_destination(u.Vector(0,18000,0))
    advance(35)
    pos=scout.get_actor_location()
    check("기지를 가로지르는 이동 명령에서 코어 앞 정지",-18000<pos.y<-5000)
    scout.set_actor_location(u.Vector(35000,35000,15000),False,True)
    check("단절 전 건조 예약",station.order_hull(2,"corvette"))
    guid=station.facility_orders(2)[0]
    check("조선소 진입 통로 철거",station.demolish_module(1))
    advance(65)
    check("단절 중 건조 중지",station.vessel_phase(guid)==0 and not station.find_vessel(guid))
    check("통로 복구",station.reserve_module("Corridor",u.IntVector(0,3,0),1))
    advance(62)
    check("재연결 후 건조 재개",station.vessel_phase(guid)==1)
    held=station.find_vessel(guid)
    scout.set_actor_location(held.get_actor_location()+u.Vector(-3200,0,0),False,True)
    scout.stop_movement()
    check("차단 출항 명령",station.launch_vessel(guid))
    advance(10)
    check("입구 차단 시 중복 출격 없음",station.vessel_phase(guid)==2 and len(pc.get_editor_property("ships"))==2)
    scout.set_actor_location(u.Vector(35000,35000,15000),False,True)
    advance(10)
    check("입구가 열리면 출항",station.vessel_phase(guid)==3)

    check("고등급 시험장 초기화",pc.start_play_scenario("BaseTrial"))
    station=base()
    scout=ship()
    scout.set_actor_location(u.Vector(160000,160000,16000),False,True)
    scout.set_destination(scout.get_actor_location())
    def reserve(kind,x,y,turn=0):
        return station.reserve_module(kind,u.IntVector(x,y,0),turn)
    def fast(seconds):
        pc.action("Speed1")
        if pc.get_editor_property("simulation_paused"):pc.action("Pause")
        pc.step_simulation(seconds)
        if not pc.get_editor_property("simulation_paused"):pc.action("Pause")
    for x,y,rotation in ((1,0,0),(0,1,1),(-1,0,2),(0,-1,3)):
        check("저장 용량 준비 "+str(rotation),reserve("Corridor",3*x,3*y,rotation) and reserve("Storage",4*x,4*y,rotation))
        fast(76)
    for tech,duration in ((1,300),(2,600),(3,960)):
        check("코어 등급 "+str(tech+1),station.prepare_trial_technology(tech) and station.upgrade_module(0))
        fast(duration+1)
    for tech in (4,5,6):check("조선 기술 해금 "+str(tech),station.prepare_trial_technology(tech))
    for n in (8,7,6,5,4,3):
        check("조선소 부지 정리 "+str(n),station.demolish_module(n))
    check("대형 조선소 건설",reserve("Corridor",0,3,1) and reserve("YardLarge",-8,4))
    fast(436)
    station.set_trial_accounting(False,True)
    check("초대형 조선소 건설",reserve("Corridor",0,-3,3) and reserve("YardSuperheavy",-11,-11))
    fast(736)
    check("초대형 조선소 중형 거절",not station.order_hull(6,"vector"))
    for hull in ("vector","corvette","frigate"):
        verify("대형 조선소 혼합 주문 "+hull,station.order_hull(4,hull))
    check("혼합 작업칸 한도",not station.order_hull(4,"corvette"))
    ids=station.facility_orders(4)
    advance(46)
    check("작업칸 사용량과 개별 시간 분리",station.vessel_phase(ids[0])==0 and all(station.vessel_phase(g)==1 for g in ids[1:]))
    advance(76)
    check("혼합 편성 실제 세 척",all(station.vessel_phase(g)==1 for g in ids) and len(pc.get_editor_property("ships"))==4)

    check("도크 설치 부지 마련",station.demolish_module(2))
    check("대형 도크 예약",reserve("DockLarge",4,-1))
    fast(720)
    dock_index=station.module_count()-1
    dev=pc.get_editor_property("development")
    pc.set_editor_property("developer_mode",True)
    test_carriers=dev.spawn_ships("carrier",0,1,u.Vector(80000,80000,18000),0,2,0,False,False)
    check("실제 함재기 재고를 가진 항모 준비",len(test_carriers)==1)
    carrier=test_carriers[0]
    wing=pc.get_editor_property("carrier")
    interior=pc.get_editor_property("interior")
    party=list(interior.register_boarding_team(carrier,0,1,4))
    def manifest():
        return sorted((c.aircraft_id,c.hp,c.shield,str(c.state)) for c in wing.aircraft if c.carrier==carrier)
    air=manifest()
    check("영웅 1명·분대원 4명·함재기 실물 준비",len(party)==5 and len(air)>0)
    verify("탑재 항모 대형 도크 예약",station.dock_vessel(dock_index,carrier))
    guid=station.vessel_id(carrier)
    for _ in range(200):
        advance(5)
        if station.vessel_phase(guid)==6:break
    verify("탑재 상태로 항모 접안",station.vessel_phase(guid)==6 and station.find_vessel(guid)==carrier)
    check("정박 중 인원·기체 보존",sorted(party)==sorted(interior.get_units_on_ship(carrier,0)) and air==manifest())
    check("탑재 명단 저장 제한 안내",not ok(station.save_trial()))
    check("탑재 항모 재출항",station.launch_vessel(guid))
    advance(15)
    check("재출항 후 동일 영웅·병력·함재기",station.vessel_phase(guid)==3 and sorted(party)==sorted(interior.get_units_on_ship(carrier,0)) and air==manifest())
    report["manifest"]={"personnel":len(party),"aircraft":len(air)}

    for speed in (1,2,3):
        check("배속 시험 초기화 "+str(speed),pc.start_play_scenario("BaseTrial"))
        station=base()
        verify("배속 시험 준비 "+str(speed),station.prepare_operations_trial() and station.order_hull(2,"corvette"))
        guid=station.facility_orders(2)[0]
        pc.action("Speed"+str(speed))
        if not pc.get_editor_property("simulation_paused"):pc.action("Pause")
        pc.step_simulation(90)
        check("정지 동안 건조 동결 "+str(speed),not station.find_vessel(guid))
        hud.execute_ui_action("BaseBack")
        pc.action("Pause")
        for _ in range(42):pc.step_simulation(1/speed)
        check("42게임초 준공 전 "+str(speed),station.vessel_phase(guid)==0)
        hud.execute_ui_action("BaseView")
        for _ in range(5):pc.step_simulation(1/speed)
        check("화면 왕복 후 47게임초 준공 "+str(speed),station.vessel_phase(guid)==1)

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
            for path,content in protected.items():
                if content is not None:
                    path.write_bytes(content)
                elif path.exists():
                    path.unlink()
            (output/"PIE.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
