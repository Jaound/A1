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
    def capture(name):
        for _ in range(8):yield
        file=output/(name+".png")
        previous=file.stat().st_mtime_ns if file.exists() else 0
        u.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+file.as_posix()+'" -nosuffix',pc)
        until=time.monotonic()+5
        while time.monotonic()<until:
            if file.exists() and file.stat().st_mtime_ns>previous:break
            yield
    for name in ("anti_ship_module","ciws_beam_b","ciws_gatling_6","heavy_cannon_shc400","laser_turret_lb30",
                 "missile_pod_mm24","missile_pod_mm48","naval_gun_bf120","plasma_heavy_cannon","railgun_rg200"):
        texture=u.load_asset("/Game/UI/MissionCards/Weapons/"+name)
        check("무장 카드 반입 "+name,isinstance(texture,u.Texture2D))
    for budget in (50,100,200,300):
        mission="Fleet"+str(budget)
        for env,kind in enumerate([u.OVSpaceEnvironmentType.NONE,u.OVSpaceEnvironmentType.BLACK_HOLE,u.OVSpaceEnvironmentType.ASTEROID_FIELD]):
            prefix=f"{budget}코스트 · {env+1}번 환경 "
            check(prefix+"일반 출격 창",hud.begin_fleet_preparation(mission,False) and hud.is_title_screen_open())
            check(prefix+"전투 격리",play.elapsed_seconds==0 and pc.simulation_paused and not pc.ships)
            title=hud.get_title_screen()
            def cmd(action,index=0):
                return play.sortie_command(action,index)
            title.sortie_click("Map",env)
            if budget==50 and env==0:yield from capture("map")
            title.sortie_click("Next")
            if budget==50 and env==0:yield from capture("empty-fleet")
            check(prefix+"빈 편성에서 시작",play.sortie_cost()==0 and play.sortie_fleet_count()==0 and not cmd("Next"))
            check(prefix+"사용자가 불러온 예산 추천",cmd("Recommend") and play.sortie_cost()==budget and play.sortie_stage==1)
            enemy_ids=[str(play.sortie_hull(i)) for i in range(play.sortie_fleet_count())]
            if budget==50 and env==0:
                catalog=[str(x) for x in play.sortie_catalog()]
                check("예산 초과 거부",not cmd("Add",catalog.index("dreadnought")))
                while play.sortie_fleet_count():cmd("Remove",0)
                check("빈 함대 출격 거부",not cmd("Next"))
                for name in ("battleship","vector","corvette"):check("혼성 편성 "+name,cmd("Add",catalog.index(name)))
            ids=[str(play.sortie_hull(i)) for i in range(play.sortie_fleet_count())]
            check(prefix+"설계 이동",cmd("Next") and play.sortie_stage==2)
            fitted=None;changed_rooms={};pd_change=None;empty_mount=None;dedicated=[]
            check(prefix+"빈 설계 출격 차단",not cmd("Next"))
            if budget==50 and env==0:
                title.sortie_click("Ship",0)
                yield from capture("empty-facilities")
            for ship_index in range(len(ids)):
                cmd("Ship",ship_index)
                proxy=play.inspect_sortie(ship_index)
                check(prefix+f"{ship_index+1}번 함선 빈 시설·무장",
                      all(play.sortie_room_at(i)==u.OVFacilityType.NONE for i in range(len(proxy.systems.layout.slots))) and
                      all(str(play.sortie_weapon_at(i))=="None" for i in range(len(proxy.systems.layout.weapon_mounts))))
                check(prefix+f"{ship_index+1}번 독립 추천",cmd("RecommendRooms") and cmd("RecommendWeapons"))
                if env==0 and ids[ship_index] in ("dreadnought","missile_ship"):
                    for j,m in enumerate(proxy.systems.layout.weapon_mounts):
                        if str(m.required_profile_id)=="None":continue
                        cmd("Slot",j)
                        wrong=next(k for k,wep in enumerate(pc.combat.settings.weapons) if wep.tier==int(m.tier) and wep.profile_id!=m.required_profile_id)
                        check("전용 무장 유지·교차 장착 거부",str(play.sortie_weapon_at(j))==str(m.required_profile_id) and not cmd("Equip",wrong))
                        dedicated.append((ship_index,str(m.weapon_id),str(m.required_profile_id),str(m.presentation_profile)))
            check(prefix+"추천 설계 유효",not str(play.sortie_problem(2)))
            if budget==50 and env==0:
                cmd("Ship",0);proxy=play.inspect_sortie(0)
                deck_two=next(i for i,s in enumerate(proxy.systems.layout.slots) if s.deck_id==2)
                original=play.sortie_room_at(deck_two)
                replacement=u.OVFacilityType.SECURITY if original==u.OVFacilityType.NONE else u.OVFacilityType.NONE
                check("새 층에서 보이는 방에 즉시 배치",cmd("Deck",2) and cmd("SetRoom",int(replacement.value)) and
                      play.sortie_room_at(deck_two)==replacement and cmd("SetRoom",int(original.value)))
            if budget==50 and env==0:
                title.sortie_click("Ship",0)
                yield from capture("fitted-facilities")
                title.sortie_click("ShowWeapons")
                yield from capture("external-mounts")
                title.sortie_click("ShowRooms")
            if budget==50 and env==0:
                cmd("Ship",2);proxy=play.inspect_sortie(2)
                core=next(i for i,s in enumerate(proxy.systems.layout.slots) if s.facility==u.OVFacilityType.CORE)
                cmd("Room",core)
                check("코어 해제와 직접 재배치",cmd("ClearRoom") and not cmd("Next") and cmd("SetRoom",int(u.OVFacilityType.CORE.value)))
                check("잘못된 교환 거부",not cmd("Swap",core))
                ammo=next(i for i,s in enumerate(proxy.systems.layout.slots) if s.facility==u.OVFacilityType.AMMO)
                cmd("Room",ammo)
                check("다른 방에 코어 장착 거부",not cmd("SetRoom",int(u.OVFacilityType.CORE.value)))
                check("함포 탄약고 누락 차단",cmd("ClearRoom") and not cmd("Next") and "탄약고" in str(play.sortie_feedback))
                check("탄약고 직접 보충",cmd("SetRoom",int(u.OVFacilityType.AMMO.value)) and not str(play.sortie_problem(2)))
                cmd("Tab")
                rejected=any(not cmd("Equip",i) for i in range(len(pc.combat.settings.weapons)))
                check("불법 장착 거부",rejected);cmd("Defaults")
                cmd("Ship",1);proxy=play.inspect_sortie(1)
                slots=list(proxy.systems.layout.slots)
                check("존재하지 않는 층 거부",not cmd("Deck",9))
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
                        if item.family==m.profile.family and item.profile_id!=m.profile.profile_id and cmd("Equip",k):
                            fitted=(str(m.mount_id),str(item.profile_id));break
                    if fitted:break
                check("호환 무장 교체",fitted is not None)
                left_empty=next(i for i,m in enumerate(proxy.systems.layout.weapon_mounts) if m.type!=u.OVWeaponType.INTERCEPTION and str(m.weapon_id)!=fitted[0])
                cmd("Slot",left_empty);check("외장 슬롯 한 곳 비워 두기",cmd("ClearMount"))
                empty_mount=str(proxy.systems.layout.weapon_mounts[left_empty].weapon_id)
                pd_index=next(i for i,m in enumerate(proxy.systems.layout.weapon_mounts) if m.type==u.OVWeaponType.INTERCEPTION)
                cmd("Slot",pd_index)
                pd_change=str(proxy.systems.layout.weapon_mounts[pd_index].weapon_id)
                check("실탄 요격포 직접 장착",cmd("ClearMount") and str(play.sortie_weapon_at(pd_index))=="None"
                      and not cmd("Equip",0) and cmd("EquipPD",1) and str(play.sortie_weapon_at(pd_index))=="PD_Kinetic")
            check(prefix+"배치 이동",cmd("Next") and play.sortie_stage==3)
            cmd("Ship",0)
            check(prefix+"아군 영역 경계",not play.set_sortie_position(u.Vector(22000,0,0),0))
            check(prefix+"함체 겹침",not play.set_sortie_position(play.sortie_location(1),0))
            if env==1:check(prefix+"블랙홀 중심 거부",not play.set_sortie_position(u.Vector(0,0,2000),0) and "블랙홀" in str(play.sortie_feedback))
            if env==2:check(prefix+"실체 겹침 차단",not play.set_sortie_position(u.Vector(-17000,23000,420),0))
            check(prefix+"세 진형 프리셋",all(cmd("Formation",preset) and not str(play.sortie_problem(3)) for preset in (1,2,0)))
            if "corvette" in ids and ("carrier" in ids or "battleship" in ids):
                rear=ids.index("carrier" if "carrier" in ids else "battleship")
                check(prefix+"균형 전열 앞쪽 초계함",play.sortie_location(ids.index("corvette")).x>play.sortie_location(rear).x)
            cmd("Auto");locations=[play.sortie_location(i) for i in range(len(ids))]
            if budget==50 and env==0:
                title.sortie_click("Ship",0)
                yield from capture("formation")
            check(prefix+"영웅 선택 이동",cmd("Next") and play.sortie_stage==4)
            check(prefix+"영웅 선택",cmd("Hero",0))
            if budget==50 and env==0:
                title.sortie_click("Detail")
                yield from capture("hero-portrait")
                check("같은 영웅은 해제·재선택",cmd("Hero",0) and cmd("Hero",0))
                check("시험 한도 적용",cmd("Hero",1) and cmd("Hero",2) and not cmd("Hero",3))
                cmd("Hero",2)
                cmd("Next");cmd("Ship",2);cmd("Room",0)
                cmd("Person",1);check("초계함 첫 영웅 탑승",cmd("Embark"))
                cmd("Person",0);check("초계함 영웅 정원 초과 거부",not cmd("Embark"))
                cmd("Prev");cmd("Hero",1)

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
                    yield from capture(name)
                title.sortie_click("Next")
            title.sortie_click("Start")
            check(prefix+"출격",str(play.active_id)==mission and not hud.is_title_screen_open())
            check(prefix+"출격 직후 일시정지",pc.simulation_paused and play.elapsed_seconds==0)
            allies=[s for s in pc.ships if s.systems.faction_id==0]
            enemies=[s for s in pc.ships if s.systems.faction_id==1]
            check(prefix+"실제 아군 함종",[str(s.hull_definition_id) for s in allies]==ids)
            check(prefix+"실제 적 공개 편성",[str(s.hull_definition_id) for s in enemies]==enemy_ids)
            check(prefix+"위치와 고도",all((s.get_actor_location()-pt).length()<1 for s,pt in zip(allies,locations)))
            check(prefix+"실제 탑승 영웅",any(unit.ship==allies[0] and unit.role!=u.OVInteriorRole.TROOPER for unit in pc.interior.units))
            if changed_rooms:
                check("실제 함선의 시설 배치",all(any(str(s.slot_id)==key and s.facility==value for s in allies[0].systems.layout.slots) for key,value in changed_rooms.items()))
                check("실제 함선의 무장",any((str(m.mount_id),str(m.profile.profile_id))==fitted for m in pc.combat.get_ship_state(allies[0]).mounts))
            for i,mount_id,profile_id,visual in dedicated:
                actual=next(m for m in allies[i].systems.layout.weapon_mounts if str(m.weapon_id)==mount_id)
                check("전용 장착점 실전 적용",str(actual.presentation_profile)==visual and
                      any(str(m.mount_id)==mount_id and str(m.profile.profile_id)==profile_id for m in pc.combat.get_ship_state(allies[i]).mounts))
            if empty_mount:
                check("비워 둔 무장은 전장에 생성되지 않음",all(str(m.mount_id)!=empty_mount for m in pc.combat.get_ship_state(allies[0]).mounts))
            if pd_change:
                check("실전에서 선택한 실탄 요격포",any(str(m.mount_id)==pd_change and m.profile.kind==u.OVPDWeaponKind.KINETIC for m in pc.point_defense.get_ship_state(allies[0]).mounts))
            check(prefix+"환경 일치",pc.active_environment==kind)
            check(prefix+"중복 출격 차단",not play.commit_sortie())
            if env==2 and budget==50:
                # 같은 위치에서 엄폐물 파괴 전후를 실제 무장 발사로 대조한다.
                for s in pc.ships:s.set_automation(False,False)
                shooter=next(s for s in allies if str(s.hull_definition_id)=="vector")
                target=next(s for s in enemies if str(s.hull_definition_id)=="carrier")
                rocks=pc.combat.environment_asteroids
                rock=rocks[0]
                center=u.Vector(rock.x,rock.y,rock.z)
                shooter.set_actor_location(center+u.Vector(-15000,0,0),False,True)
                target.set_actor_location(center+u.Vector(15000,0,0),False,True)
                shooter.set_actor_rotation(u.Rotator(0,0,0),False)
                pc.combat.set_target(shooter,target)
                shooter.set_automation(False,True)
                before=len(rocks)
                for _ in range(1200):
                    pc.combat.simulate(.1)
                    if len(pc.combat.environment_asteroids)<before:break
                check("실제 발사로 엄폐 소행성 파괴",len(pc.combat.environment_asteroids)<before)
                check("차폐된 탄은 소행성 표면에서 종료",any(str(e.attacker_id)==shooter.get_name() and abs((e.position-center).length()-rock.w)<100 for e in pc.combat.recent_events))
                for _ in range(1200):
                    pc.combat.simulate(.1)
                    if any(e.attacker==shooter and str(e.hit_ship_id)==target.get_name() for e in pc.combat.recent_events):break
                check("엄폐 파괴 후 같은 사선에서 적함 피격",any(e.attacker==shooter and str(e.hit_ship_id)==target.get_name() for e in pc.combat.recent_events))
            if env==2:check(prefix+"소행성 파괴",pc.damage_environment_asteroid(0,100000))
            if budget==50 and env==0:
                pc.action("RestartScene")
                check("같은 편성으로 일시정지 재시작",pc.simulation_paused and play.elapsed_seconds==0 and [str(s.hull_definition_id) for s in pc.ships if s.systems.faction_id==0]==ids)
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
