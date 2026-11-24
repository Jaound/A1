"""ControlRange PIE. Build a real trial, place each socket at all quarter turns, and sweep the actual corvette envelope."""
import json
import os
import time
import traceback
from pathlib import Path
import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
folder=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/"Validation"/"BasePorts"
folder.mkdir(parents=True,exist_ok=True)
records=json.loads((Path(os.environ["OV_BASE_STAGE3_PREPARED"])/"FittedPorts"/"ports.json").read_text())
names=dict(LogisticsTerminal="Logistics",ResearchLab="Research",HeroAcademy="Academy",CrewSupport="Crew",
           DefensePlatform="Defense",RadarArray="Radar",ShieldBastion="Shield")
report=dict(seams=[],bays=[],ui=[],failures=[])
def need(ok,label):
    if not ok: raise ValueError(label)
def rotate(x,y,turn):
    for _ in range(turn): x,y=-y,x
    return x,y

def run():
    deadline=time.monotonic()+160
    while True:
        world=editor.get_game_world()
        pc=u.GameplayStatics.get_player_controller(world,0) if world else None
        if pc and pc.get_hud(): break
        if time.monotonic()>deadline: raise TimeoutError("PIE")
        yield
    need(pc.start_play_scenario("BaseTrial"),"임무 시작")
    hud=pc.get_hud()
    base,=u.GameplayStatics.get_all_actors_of_class(world,u.OVBasePresentation)
    ship,=pc.get_editor_property("ships")
    away=u.Vector(200000,200000,20000)
    ship.set_actor_location(away,False,True)
    ship.set_destination(away)
    def elapsed(sec):
        pc.action("Speed1")
        if pc.get_editor_property("simulation_paused"): pc.action("Pause")
        pc.step_simulation(sec)
        if not pc.get_editor_property("simulation_paused"): pc.action("Pause")
    def parts():
        return {c.static_mesh.get_name():c for c in base.get_components_by_class(u.StaticMeshComponent) if c.static_mesh}
    def reserve(kind,x,y,turn):
        return base.reserve_module(kind,u.IntVector(x,y,0),turn)
    # Free trial still enforces storage headroom. Four independent stores allow IV.
    for turn in range(4):
        dx,dy=rotate(1,0,turn)
        need(reserve("Corridor",3*dx,3*dy,turn),"시험 통로")
        need(reserve("Storage",4*dx,4*dy,turn),"시험 창고")
        elapsed(76)
    for tech,duration in [(1,300),(2,600),(3,960)]:
        need(base.prepare_trial_technology(tech) and base.upgrade_module(0),"코어 등급")
        elapsed(duration+1)
    for tech in (4,5,6):
        need(base.prepare_trial_technology(tech),"함체 연구")
    for i in range(8,0,-1): need(base.demolish_module(i),"시험 설비 회수")
    hull=next(c for c in ship.get_components_by_class(u.StaticMeshComponent)
              if c.static_mesh and c.static_mesh.get_name()=="SM_PC14_FullMesh")
    bounds=hull.static_mesh.get_bounding_box()
    ext=(bounds.max-bounds.min)*hull.get_world_scale()*.5
    half=u.Vector(*sorted([abs(ext.x),abs(ext.y),abs(ext.z)],reverse=True))
    report["corvette_half_cm"]=[half.x,half.y,half.z]
    # Place the corridor with XP facing the selected module face.
    dirs={"XN":(1,0,0),"XP":(-1,0,2),"YN":(0,1,1),"YP":(0,-1,3)}
    verified_rims={}
    photo=os.environ.get("OV_PORT_PHOTOS")=="1"
    if photo:
        camera,=u.GameplayStatics.get_all_actors_with_tag(world,"OVPortInspection")
        light,=u.GameplayStatics.get_all_actors_with_tag(world,"OVPortLight")
        cap=camera.capture_component2d
        rt=u.RenderingLibrary.create_render_target2d(world,900,650,u.TextureRenderTargetFormat.RTF_RGBA8)
        cap.texture_target=rt
        cap.capture_source=u.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        cap.fov_angle=50
        cap.capture_every_frame=False
        cap.capture_on_movement=False
    for name,data in records.items():
        for key,ref in data.items():
            if key=="bay_trials": continue
            for turn in range(4):
                dx,dy,linkturn=dirs[key]
                dx,dy=rotate(dx,dy,turn)
                vx,vy,_=ref["face_cm"]
                cx,cy=rotate(round(vx/2500+dirs[key][0]*.5),round(vy/2500+dirs[key][1]*.5),turn)
                need(reserve("Corridor",dx*3,dy*3,(linkturn+turn)%4),"통로 "+name+key)
                need(reserve(names.get(name,name),4*dx-cx,4*dy-cy,turn),"예약 "+name+key+str(turn))
                components=parts()
                body=components["SM_OV_Base_"+name]
                sleeve=components["SM_OV_Ports_"+name]
                corridor=components["SM_OV_Base_CorridorStraight_1x1x1"]
                p=body.get_socket_location(key)
                q=corridor.get_socket_location("XP")
                a=body.get_socket_rotation(key)
                b=corridor.get_socket_rotation("XP")
                gap=(p-q).length()
                normal=u.MathLibrary.get_forward_vector(a).dot(u.MathLibrary.get_forward_vector(b))
                roll=u.MathLibrary.get_up_vector(a).dot(u.MathLibrary.get_up_vector(b))
                transform=sleeve.get_world_transform()
                face=u.MathLibrary.transform_location(transform,u.Vector(*ref["face_cm"]))
                offset=(p-face).length()
                need(gap<.5 and offset<.5 and normal<-.999 and roll>.999,"접합 "+name+key)
                report["seams"].append([name,key,turn,round(gap,4),round(offset,4),round(normal,5),round(roll,5)])
                if (name,key) not in verified_rims:
                    vertices=u.ProceduralMeshLibrary.get_section_from_static_mesh(sleeve.static_mesh,0,0)[0]
                    local=u.Vector(*ref["face_cm"])
                    across=u.Vector(0,1,0) if key.startswith("X") else u.Vector(1,0,0)
                    ring=[(-.55,-1),(.55,-1),(1,-.55),(1,.55),(.55,1),(-.55,1),(-1,.55),(-1,-.55)]
                    error=max(min((local+across*(218*x)+u.Vector(0,0,268*z)-v).length()
                                  for v in vertices) for x,z in ring)
                    verified_rims[name,key]=error
                    need(error<.5,"팔각 림의 실제 LOD0 꼭짓점 "+name+key+str(error))
                    report.setdefault("rim_error_cm",{})[name+"."+key]=round(error,4)
                if key==next(iter(data)) and "bay_trials" in data:
                    path=None
                    # Allow component registration and asynchronous mesh collision cooking to finish.
                    for _ in range(6): yield
                    counts=dict(entrance=0,exit=0,missing_floor=0,other_floor=0)
                    heading=turn*90+(0 if name.startswith("Yard") else 90)
                    def sweep(start,end):
                        return u.SystemLibrary.box_trace_single(world,start,end,half,u.Rotator(yaw=heading),
                            u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],u.DrawDebugTrace.NONE,False)
                    for candidate in data["bay_trials"]:
                        start,end,floor=[u.MathLibrary.transform_location(transform,u.Vector(*candidate[k])) for k in ("start","end","floor")]
                        incoming=sweep(start,end)
                        if incoming:
                            counts["entrance"]+=1
                            continue
                        if sweep(end,start):
                            counts["exit"]+=1
                            continue
                        wall=sweep(end,floor-u.Vector(0,0,250))
                        if not wall:
                            counts["missing_floor"]+=1
                            continue
                        if body in wall.to_tuple():
                            path=candidate
                            break
                        counts["other_floor"]+=1
                    report["bays"].append(dict(model=name,turn=turn,path=path,counts=counts))
                    if not path: report["failures"].append(name+" 출입/바닥 "+str(turn))
                if photo and turn==1:
                    elapsed(850)
                    need(base.remaining_seconds(2)==0,"촬영 전 완공")
                    n=u.MathLibrary.get_forward_vector(a)
                    side=u.MathLibrary.get_right_vector(a)
                    shell=u.MathLibrary.transform_location(transform,u.Vector(*ref["shell_cm"]))
                    aim=(p+shell)*.5
                    radius=max(1400.,(p-shell).length()*1.4)
                    eye=aim+n*(radius*.28)+side*(radius*1.5)+u.Vector(0,0,radius*.80)
                    cap.primitive_render_mode=u.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST
                    cap.clear_show_only_components()
                    for component in (body,sleeve,corridor): cap.show_only_component(component)
                    camera.set_actor_location_and_rotation(eye,u.MathLibrary.find_look_at_rotation(eye,aim),False,False)
                    need((cap.get_world_location()-eye).length()<1.,"촬영 위치")
                    light.set_actor_location(eye,False,True)
                    for _ in range(8): yield
                    cap.capture_scene()
                    yield
                    u.RenderingLibrary.export_render_target(world,rt,str(folder),name+"_"+key+".png")
                    need(base.demolish_module(2) and base.demolish_module(1),"촬영 설비 회수")
                else:
                    need(base.cancel_construction(2) and base.cancel_construction(1),"공사 해제")
                yield
    # HUD draw frames are necessary for hitboxes. Use the normal action router.
    for _ in range(6): yield
    for action in ("BaseFill","BasePrevious","BaseTech_1","BaseSave"):
        need(hud.get_action_center(action).x<0,"기본 화면 "+action)
    report["ui"].append("기본 측면 창 없음")
    for action in ("BaseOperations","BaseBuild","BaseResearch","BaseFiles"): hud.execute_ui_action(action)
    for _ in range(6): yield
    for action in ("BaseFill","BasePrevious","BaseTech_1","BaseSave"):
        need(hud.get_action_center(action).x>0,"동시 창 "+action)
    report["ui"].append("네 창 동시 열기")
    hud.execute_ui_action("BaseWin1_Pin")
    hud.execute_ui_action("BaseWin2_Min")
    hud.execute_ui_action("BaseWin3_Close")
    for _ in range(6): yield
    need(hud.get_action_center("BaseTech_1").x<0 and hud.get_action_center("BaseSave").x<0
         and hud.get_action_center("BasePrevious").x>0,"독립 창 조작")
    report["ui"].append("최소화·닫기 후 다른 창 유지")

def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        pass
    except Exception:
        report["failures"].append(traceback.format_exc())
    report["passed"]=not report["failures"] and len(report["seams"])==80 and len(report["bays"])==24
    (folder/"PIE.json").write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding="utf8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()

if os.environ.get("OV_PORT_PHOTOS")=="1":
    # This actor is duplicated into the trial world by PIE. The editor map is never saved.
    probe=u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.SceneCapture2D,u.Vector(0,0,1000))
    probe.tags=["OVPortInspection"]
    probe.root_component.set_mobility(u.ComponentMobility.MOVABLE)
    lamp=u.get_editor_subsystem(u.EditorActorSubsystem).spawn_actor_from_class(u.PointLight,u.Vector(0,0,1000))
    lamp.tags=["OVPortLight"]
    lamp.root_component.set_mobility(u.ComponentMobility.MOVABLE)
    lamp.point_light_component.set_editor_property("use_inverse_squared_falloff",False)
    lamp.point_light_component.set_intensity(6.)
    lamp.point_light_component.set_attenuation_radius(28000.)
    lamp.point_light_component.set_cast_shadows(False)
job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
