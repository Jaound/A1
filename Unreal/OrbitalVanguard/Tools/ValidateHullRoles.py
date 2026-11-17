"""16종 실제 DEV 생성의 제한된 PIE 역할 검사. 렌더·마우스·최종 밸런스 검증 아님."""
import json, math, os, time, traceback, unreal

ROOT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
OUT = os.path.join(ROOT, "Validation", "HullRoles.json")
os.makedirs(os.path.dirname(OUT), exist_ok=True)
report = {"passed": None, "planning": "035a4fba81d26f1d67daa1f44bded0f7e8d3a434",
          "scope": "실제 DEV 생성·선택·이동·대표 무장 사격·피해·시설·완파·정리; 방공 비활성·동일 표적; 물리 입력/최종 밸런스 제외",
          "checks": [], "cases": [], "errors": []}
state = {"phase": 0, "start": time.monotonic()}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
HULLS = ["corvette", "frigate", "vector", "bastion", "cruiser", "air_defense", "radar", "battleship", "carrier", "missile_ship", "dreadnought", "battlecarrier", "repair", "mining", "transport", "assault"]
SUPPORT = {"repair", "mining", "transport", "assault"}


def p(o, k): return o.get_editor_property(k)
def check(name, result, detail=""):
    report["checks"].append({"id": name, "passed": bool(result), "detail": str(detail)})
    unreal.log("OVHULL %s %s %s" % (name, bool(result), detail))
def save():
    report["passed"] = bool(report["checks"]) and all(c["passed"] for c in report["checks"]) and not report["errors"]
    with open(OUT, "w", encoding="utf-8") as f: json.dump(report, f, ensure_ascii=False, indent=2)
def pause(pc, value):
    if p(pc, "simulation_paused") != value: pc.action("Pause")
def advance(pc, seconds):
    pause(pc, False); pc.step_simulation(seconds); pause(pc, True)
def distance(a,b): return math.sqrt((a.x-b.x)**2+(a.y-b.y)**2+(a.z-b.z)**2)
def prepare(pc):
    if not pc.start_calibration_range(1): raise RuntimeError("F6 scene initialization failed")
    pc.set_editor_property("developer_mode", True)
    combat = p(pc, "combat")
    for ship in p(pc, "ships"):
        ship.set_automation(False, False)
        combat.notify_ftl_departure(ship)
    return p(pc, "development"), combat

def spawn(dev, key, faction, pos, inventory=0):
    values = dev.spawn_ships(key, faction, 1, pos, 0, inventory, 0, False, False)
    return values[0] if len(values) == 1 else None

def mount_type(m): return p(m,"type")

def run_hull(pc, key):
    dev, combat = prepare(pc)
    attacker = spawn(dev, key, 0, unreal.Vector(-140000, 100000, 2000))
    check(key + " / 실제 생성", attacker is not None, p(dev,"last_message"))
    if not attacker: return
    target = spawn(dev, "battleship", 1, unreal.Vector(-80000, 100000, 2000))
    check(key + " / 동일 표적 생성", target is not None, p(dev,"last_message"))
    if not target: return
    p(pc,"point_defense").disable()
    check(key + " / PD 배제", not p(p(pc,"point_defense"),"enabled"))
    attacker.set_automation(False,False); target.set_automation(False,False)
    pc.select_ship(attacker)
    check(key + " / 선택", p(pc,"selected") == attacker)
    original = attacker.get_actor_location()
    attacker.set_destination(unreal.Vector(original.x + 3000, original.y, original.z))
    advance(pc,2)
    now = attacker.get_actor_location()
    check(key + " / 실제 이동", distance(original,now)>1, distance(original,now))
    attacker.stop_movement(); advance(pc,.25)
    system=p(attacker,"systems"); layout=p(system,"layout")
    mounts=p(combat.get_ship_state(attacker),"mounts")
    case={"hull": key, "runtime_id":attacker.get_name(), "representative_weapon":None, "shots":0, "hits":0,
          "pd":"비활성", "target":"battleship", "condition":"초기 중심 거리 60000cm, 동일 고정 표적; DEV 기체 재고 0", "cooldown_check":"미실시"}
    if key not in SUPPORT:
        check(key + " / 공격 무장 존재", len(mounts)>0, len(mounts))
        if mounts:
            mid=p(mounts[0],"mount_id"); case["representative_weapon"]=str(mid)
            check(key + " / 표적 지정", dev.set_ship_target(attacker,target), p(dev,"last_message"))
            fired_request=dev.fire_weapon(attacker,mid)
            check(key + " / 발사 요청", fired_request, p(dev,"last_message"))
            name=attacker.get_name(); matched=[]; launch_times={}
            for frame in range(120):
                advance(pc,.25)
                matched=[e for e in p(combat,"recent_events") if str(p(e,"attacker_id"))==name and str(p(e,"weapon_id"))==str(mid)]
                flying=[e for e in p(combat,"attacks") if str(p(e,"attacker_id"))==name and str(p(e,"weapon_id"))==str(mid)]
                for e in matched+flying:
                    launch_times[p(e,"attack_id")]=p(e,"launch_time")
                hits=[e for e in matched if p(e,"result")==unreal.OVAttackResult.HIT and str(p(e,"hit_ship_id"))==target.get_name()]
                if hits and len(launch_times)>=2: break
                if frame % 4 == 3: dev.fire_weapon(attacker,mid)
            current=combat.get_ship_state(attacker)
            row=next((m for m in p(current,"mounts") if p(m,"mount_id")==mid),None)
            shots=p(row,"shots_fired") if row else 0
            hits=[e for e in matched if p(e,"result")==unreal.OVAttackResult.HIT and str(p(e,"hit_ship_id"))==target.get_name()]
            case["shots"],case["hits"]=shots,len(hits)
            times=sorted(launch_times.values())
            if row and len(times)>=2:
                interval=p(p(row,"profile"),"interval")
                salvo=p(p(row,"profile"),"salvo")
                if salvo==1:
                    min_gap=min(b-a for a,b in zip(times,times[1:]))
                    case["cooldown_check"]={"interval":interval,"minimum_observed":min_gap,"launches":len(times)}
                    check(key + " / 재요청 중 발사 간격", min_gap+0.02>=interval, case["cooldown_check"])
                else:
                    case["cooldown_check"]="살보 무장: 개별 탄 간격과 재장전 분리 추가 검사 필요"
            check(key + " / 실제 발사", shots>0, shots)
            check(key + " / 동일 표적 실제 피격", len(hits)>0, "표본 결과="+str([str(p(e,"result")) for e in matched]))
    else:
        check(key + " / 공격 무장 없음", len(mounts)==0, len(mounts))
        if key=="assault":
            pd_count=sum(mount_type(m)==unreal.OVWeaponType.INTERCEPTION for m in p(layout,"weapon_mounts"))
            check("assault / PD 1기", pd_count==1, pd_count)
            check("assault / 수송선 등록", any(p(x,"ship")==attacker for x in p(p(pc,"transport"),"shuttles")))
        else:
            check(key + " / 자원 시설 가동", system.can_use_resources())
    # Actual facility damage and operation loss, followed by explicit core destruction.
    engines=[s for s in p(layout,"slots") if p(s,"facility")==unreal.OVFacilityType.ENGINE]
    check(key + " / 엔진 1개 및 가동", len(engines)==1 and system.can_move(), len(engines))
    if engines:
        slot=p(engines[0],"slot_id"); before=p(system.get_slot_state(slot),"hp")
        system.debug_destroy_slot(slot, unreal.OVDamageCause.SABOTAGE)
        check(key + " / 실제 시설 피해", before>0 and p(system.get_slot_state(slot),"hp")==0)
        check(key + " / 엔진 기능 정지", not system.can_move())
    system.debug_destroy_slot(p(layout,"core_slot_id"), unreal.OVDamageCause.SABOTAGE)
    check(key + " / 코어 완파", p(system,"wrecked") and p(system,"wreck_event_count")==1)
    check(key + " / DEV 정리", dev.delete_ship(attacker), p(dev,"last_message"))
    check(key + " / 삭제 등록 정리", attacker not in p(dev,"spawned_ships"))
    report["cases"].append(case)

def get_logistics(world):
    # UE 5.8 does not expose the direct get_world_subsystem Python helper.
    # The reflected object iterator selects the subsystem owned by this PIE world.
    try:
        for item in unreal.ObjectIterator(unreal.OVLogisticsSubsystem):
            if item.get_outer() == world: return item
    except Exception as exc:
        unreal.log_warning("OVHULL logistics lookup: " + str(exc))
    return None

def run_support(pc, world):
    dev, combat=prepare(pc); logistics=get_logistics(world)
    check("지원 / 실제 물류 서브시스템 조회", logistics is not None, "Python world subsystem 접근")
    if not logistics: return
    miner=spawn(dev,"mining",0,unreal.Vector(-140000,100000,2000))
    transport=spawn(dev,"transport",0,unreal.Vector(-135000,100000,2000))
    repair=spawn(dev,"repair",0,unreal.Vector(-130000,100000,2000))
    target=spawn(dev,"corvette",0,unreal.Vector(-125000,100000,2000))
    check("지원 / 네 함선 실제 생성", all([miner,transport,repair,target]), p(dev,"last_message"))
    if not all([miner,transport,repair,target]): return
    node=logistics.add_dev_node(miner.get_actor_location(),15)
    mined=logistics.dev_mine(miner,node,20,100)
    check("채굴 / 유한 자원 차감", abs(mined-15)<.01 and logistics.dev_mine(miner,node,1,100)==0, mined)
    moved=logistics.dev_transfer(miner,transport,15,12000)
    check("수송 / 채굴 재고 수거", abs(moved-15)<.01,moved)
    moved2=logistics.dev_transfer(transport,repair,15,12000)
    check("수송 / 수리함 재고 이전", abs(moved2-15)<.01,moved2)
    target_system=p(target,"systems"); target_layout=p(target_system,"layout")
    engine=next(s for s in p(target_layout,"slots") if p(s,"facility")==unreal.OVFacilityType.ENGINE)
    target_system.debug_destroy_slot(p(engine,"slot_id"),unreal.OVDamageCause.SABOTAGE)
    damaged=p(target_system,"hull_hp")
    started=logistics.start_dev_repair(repair,target,False,unreal.Guid(),unreal.Vector(.1,0,0),12000,0,0,0)
    check("수리 / 유한 자원 링크 시작", started,p(logistics,"last_reason"))
    if started:
        old=next(p(c,"amount") for c in p(logistics,"cargo") if p(c,"ship")==repair)
        advance(pc,4)
        new=next(p(c,"amount") for c in p(logistics,"cargo") if p(c,"ship")==repair)
        check("수리 / 실제 복구·자원 차감", p(target_system,"hull_hp")>damaged and new<old, (old,new,damaged,p(target_system,"hull_hp")))

def run_carrier(pc,key):
    dev,combat=prepare(pc); carrier=p(pc,"carrier")
    ship=spawn(dev,key,0,unreal.Vector(-140000,100000,2000),2)
    check(key+" / 함재기 모함 생성",ship is not None,p(dev,"last_message"))
    if not ship: return
    craft=[x for x in p(carrier,"aircraft") if p(x,"carrier")==ship]
    ids={p(x,"aircraft_id") for x in craft}
    launched=carrier.launch(ship)
    check(key+" / 실제 출격",len(ids)>0 and launched==len(ids),(len(ids),launched))
    carrier.recall(ship)
    for _ in range(80):
        advance(pc,.25)
        if carrier.count_aircraft(ship,unreal.OVAircraftState.STORED)==len(ids): break
    after=[x for x in p(carrier,"aircraft") if p(x,"carrier")==ship]
    check(key+" / 실제 복귀와 동일 기체 재고",len(after)==len(ids) and {p(x,"aircraft_id") for x in after}==ids and carrier.count_aircraft(ship,unreal.OVAircraftState.STORED)==len(ids),
          (len(ids),carrier.count_aircraft(ship,unreal.OVAircraftState.STORED)))

def execute(pc,world):
    for key in HULLS:
        try: run_hull(pc,key)
        except Exception:
            report["errors"].append(key+"\n"+traceback.format_exc()); unreal.log_error(report["errors"][-1])
    for name, fn in [("support",lambda:run_support(pc,world)),("carrier",lambda:run_carrier(pc,"carrier")),("battlecarrier",lambda:run_carrier(pc,"battlecarrier"))]:
        try: fn()
        except Exception:
            report["errors"].append(name+"\n"+traceback.format_exc()); unreal.log_error(report["errors"][-1])

def tick(_dt):
    try:
        if state["phase"]==0:
            level.editor_request_begin_play(); state["phase"]=1; return
        world=editor.get_game_world()
        if not world:
            if time.monotonic()-state["start"]>60: raise RuntimeError("PIE 준비 시간 초과")
            return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        if not pc or not isinstance(pc,unreal.OVController) or not p(pc,"development"):
            if time.monotonic()-state["start"]>60: raise RuntimeError("개발 컨트롤러 준비 시간 초과")
            return
        execute(pc,world); save(); unreal.unregister_slate_post_tick_callback(state["handle"])
    except Exception:
        report["errors"].append(traceback.format_exc()); save(); unreal.log_error(report["errors"][-1]); unreal.unregister_slate_post_tick_callback(state["handle"])
state["handle"]=unreal.register_slate_post_tick_callback(tick)
