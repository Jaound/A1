"""ControlRange의 실제 PIE 임무 수명·시간·소켓을 검사한다. 마우스 검사는 별도로 수행한다."""
import json
import traceback
import time
from pathlib import Path
import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
folder=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/'Validation/BaseTrial'
folder.mkdir(parents=True,exist_ok=True)
report={'checks':[]}
def check(label, condition):
    if not condition: raise AssertionError(label)
    report['checks'].append(label)

def capture(world,pc,name):
    path=folder/(name+'.png')
    previous=path.stat().st_mtime_ns if path.exists() else 0
    for attempt in range(3):
        settle=time.monotonic()+.7
        while time.monotonic()<settle: yield
        u.SystemLibrary.execute_console_command(world,'Shot filename="'+str(path)+'" -nosuffix',pc)
        timeout=time.monotonic()+3
        while time.monotonic()<timeout:
            yield
            if path.exists() and path.stat().st_mtime_ns>previous: return
    raise RuntimeError('렌더 캡처 갱신 실패 '+name)

def run():
    while editor.get_game_world() is None: yield
    world=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(world,0)
    while not pc or not pc.get_hud():
        yield
        pc=u.GameplayStatics.get_player_controller(world,0)
    hud=pc.get_hud()
    get=lambda obj,key:obj.get_editor_property(key)
    bases=lambda:u.GameplayStatics.get_all_actors_of_class(world,u.OVBasePresentation)
    check('정상 임무 시작',pc.start_play_scenario('BaseTrial'))
    base,=bases()
    ship,=get(pc,'ships')
    check('아군 1척, 코어와 자원',get(ship,'friendly') and base.module_count()==1 and base.common_stock()==1200)
    for key in ('CoreHub_5x5x5','CorridorStraight_1x1x1','CorridorHub_1x1x1','StorageVault','YardSmall','DockStandard'):
        path='/Game/Presentation/Base/'
        mesh=u.load_asset(path+'SM_OV_Base_'+key)
        check(key+' 근거리·원거리 메시',mesh and mesh.get_num_lods()==2 and mesh.get_num_triangles(0)>1000000 and mesh.get_num_triangles(1)<81000)
        mat=u.load_asset(path+key+'/M_Surface')
        materials=[mesh.get_material(i) for i in range(len(get(mesh,'static_materials')))]
        check(key+' 원본 표면과 건설 셰이더',mat in materials and all(m and str(m.get_path_name()).startswith(path) and
              'BuildHeight' in map(str,u.MaterialEditingLibrary.get_scalar_parameter_names(m)) for m in materials))
        textures=[u.load_asset(path+key+'/T_'+suffix) for suffix in ('color','normal','metallic','roughness')]
        expected=[8192,4096,4096,4096]
        deadline=time.monotonic()+25
        while [t.blueprint_get_size_x() for t in textures]!=expected and time.monotonic()<deadline: yield
        actual=[t.blueprint_get_size_x() for t in textures]
        report.setdefault('loaded_texture_widths',{})[key]=actual
        check(key+' 원본 텍스처 해상도 '+str(actual),actual==expected)
    view=pc.get_view_target()
    check('기지와 함대가 같은 카메라 사용',view==get(pc,'range_camera'))
    def frame(focus,offset):
        rot=u.MathLibrary.find_look_at_rotation(focus+offset,focus)
        for k,v in [('camera_focus',focus),('camera_yaw',rot.yaw),('camera_pitch',rot.pitch),('camera_distance',offset.length())]:
            pc.set_editor_property(k,v)
        pc.update_camera()
    original=ship.get_actor_location()
    cell=base.get_actor_location()+u.Vector(7500,0,0)
    ship.set_actor_location(cell,False,True)
    check('초계함이 점유한 연결 가능 칸 예약 차단',not base.reserve_module('Corridor',u.IntVector(3,0,0),0) and base.module_count()==1)
    ship.set_actor_location(cell+u.Vector(0,0,7000),False,True)
    check('같은 평면 좌표에서 함선 고도 분리 후 예약',base.reserve_module('Corridor',u.IntVector(3,0,0),0))
    check('무료 시험 예약 취소 잔액 불변',base.cancel_construction(1) and base.common_stock()==1200)
    ship.set_actor_location(cell,False,True)
    hud.execute_ui_action('BaseBack')
    ship.set_destination(original)
    pc.action('Pause')
    for _ in range(300):
        pc.step_simulation(.1)
        if (ship.get_actor_location()-original).length()<75: break
    pc.action('Pause')
    check('함대 이동 명령으로 선체가 공사 공간을 비움',(ship.get_actor_location()-cell).length()>3000)
    hud.execute_ui_action('BaseView')
    def reserve(name,x,y,z,turn=0):
        return base.reserve_module(name,u.IntVector(x,y,z),turn)
    check('겹침 차단',not reserve('Storage',0,0,0))
    check('방향 차단',not reserve('Corridor',3,0,0,1))
    check('공중 고립 차단',not reserve('Corridor',3,0,1))
    check('코어 통로 창고 예약',reserve('Corridor',3,0,0) and reserve('Storage',4,0,0))
    ship.set_actor_location(cell,False,True)
    ship.set_destination(cell)
    pc.action('Pause')
    pc.step_simulation(2)
    pc.action('Pause')
    check('예약 후 진입한 함선 때문에 실제 공사도 대기',base.remaining_seconds(1)==15 and base.remaining_seconds(2)==60)
    ship.set_actor_location(original,False,True)
    ship.set_destination(original)
    check('종속 공사 보호',not base.cancel_construction(1))
    check('별도 연결 슬롯',reserve('Junction',-3,0,0) and base.common_stock()==1200)
    # 렌더 파생 이미지와 배치 검사는 별개다. 일시정지 상태에서 셰이더와 카메라가 적용될 프레임을 준다.
    settle=time.monotonic()+6
    while time.monotonic()<settle: yield
    u.SystemLibrary.execute_console_command(world,'Shot filename="'+str(folder/'BaseTrial.png')+'" -nosuffix',pc)
    for _ in range(5): yield
    parts=base.get_components_by_class(u.StaticMeshComponent)
    part=lambda key:next(c for c in parts if key in get(c,'static_mesh').get_name())
    align=[]
    for left,lp,right,rp in [('CoreHub','XP','Straight','XN'),('Straight','XP','Storage','XN'),('CoreHub','XN','CorridorHub','XP')]:
        a,b=part(left),part(right)
        check(left+' 원본 소켓 '+right,a.does_socket_exist(lp) and b.does_socket_exist(rp))
        d=(a.get_socket_location(lp)-b.get_socket_location(rp)).length()
        ra,rb=a.get_socket_rotation(lp),b.get_socket_rotation(rp)
        normal=u.MathLibrary.get_forward_vector(ra).dot(u.MathLibrary.get_forward_vector(rb))
        up=u.MathLibrary.get_up_vector(ra).dot(u.MathLibrary.get_up_vector(rb))
        align.append([left,right,d,normal,up])
        check(left+' 접합 중심·방향·상하 '+right,d<=5 and normal<-.999 and up>.999)
    report['seams_cm_dot']=align
    check('미완공 허브 취소 잔액 유지',base.cancel_construction(3) and base.common_stock()==1200)
    pc.step_simulation(20)
    check('일시정지 공사 동결',base.remaining_seconds(1)==15 and base.remaining_seconds(2)==60)
    pose=view.get_actor_transform()
    hud.execute_ui_action('BaseBack')
    check('전환 시 카메라 위치 연속', (view.get_actor_location()-pose.translation).length()<.01)
    check('두 화면 모두 기지와 함선 표시',not get(base,'hidden') and not get(ship,'hidden'))
    check('함대 카메라 복원',pc.get_view_target()==get(pc,'range_camera'))
    destination=ship.get_actor_location()+u.Vector(17000,1200,480)
    ship.set_destination(destination)
    hud.execute_ui_action('BaseView')
    check('기존 함선 명령 보존',get(ship,'has_destination') and (get(ship,'destination')-destination).length()<1)
    check('기지 카메라 재사용',pc.get_view_target()==view)
    hud.execute_ui_action('BaseBack')
    pc.action('Pause')
    for speed in [1,2,3]:
        pc.action('Speed'+str(speed))
        pc.step_simulation(1)
        check(str(speed)+'배 함대 화면 작업',base.remaining_seconds(1)==15-sum(range(1,speed+1)) and base.remaining_seconds(2)==60)
    pc.step_simulation(4)
    hud.execute_ui_action('BaseView')
    check('통로 완공 후 창고 진행',base.remaining_seconds(1)==0 and base.remaining_seconds(2)==57 and pc.get_view_target()==view)
    pc.step_simulation(19)
    check('창고 완공, 계속 플레이',base.remaining_seconds(2)==0 and get(pc.get_play_scenario(),'result')==u.OVPlaySceneResult.RUNNING)
    hud.execute_ui_action('RestartScene')
    fresh,=bases()
    check('재시작 코어·자원·대기열 초기화',fresh!=base and fresh.common_stock()==1200 and fresh.module_count()==1)
    check('재시작 시험 함선 교체',len(get(pc,'ships'))==1 and get(pc,'ships')[0]!=ship)
    check('돌린 허브 예약',fresh.reserve_module('Junction',u.IntVector(0,3,0),1))
    check('수직 상하 정렬 차단',not fresh.reserve_module('Junction',u.IntVector(0,3,1),0))
    check('수직 상하 정렬 접합',fresh.reserve_module('Junction',u.IntVector(0,3,1),1))
    hubs=[c for c in fresh.get_components_by_class(u.StaticMeshComponent) if 'CorridorHub' in get(c,'static_mesh').get_name()]
    hubs.sort(key=lambda c:c.get_socket_location('ZP').z)
    bottom,top=hubs
    ra,rb=bottom.get_socket_rotation('ZP'),top.get_socket_rotation('ZN')
    check('허브 수직 실제 메시 소켓', (bottom.get_socket_location('ZP')-top.get_socket_location('ZN')).length()<5 and
          u.MathLibrary.get_forward_vector(ra).dot(u.MathLibrary.get_forward_vector(rb))<-.999 and
          u.MathLibrary.get_up_vector(ra).dot(u.MathLibrary.get_up_vector(rb))>.999)
    vertical=(bottom.get_socket_location('ZP')-top.get_socket_location('ZN')).length()
    report['vertical_cm']=vertical
    old=pc.get_view_target()
    cam=get(pc,'range_camera')
    loc,rot=cam.get_actor_location(),cam.get_actor_rotation()
    seam=bottom.get_socket_location('ZP')
    eye=seam+u.Vector(2200,1900,850)
    pc.action('Pause')
    pc.step_simulation(31)
    pc.action('Pause')
    frame(seam,eye-seam)
    pc.set_view_target_with_blend(cam)
    u.SystemLibrary.execute_console_command(world,'ShowHUD',pc)
    for _ in range(16): yield
    u.SystemLibrary.execute_console_command(world,'Shot filename="'+str(folder/'VerticalHub.png')+'" -nosuffix',pc)
    for _ in range(6): yield
    pc.set_view_target_with_blend(old)
    cam.set_actor_location_and_rotation(loc,rot,False,False)
    u.SystemLibrary.execute_console_command(world,'ShowHUD',pc)
    check('25m층 90도 도크',fresh.reserve_module('Dock',u.IntVector(1,4,1),1))
    rotated=fresh.get_components_by_class(u.StaticMeshComponent)
    rotated_dock=next(c for c in rotated if 'DockStandard' in get(c,'static_mesh').get_name())
    upper_hub=max((c for c in rotated if 'CorridorHub' in get(c,'static_mesh').get_name()),key=lambda c:c.get_world_location().z)
    report['rotated_dock_cm']=(upper_hub.get_socket_location('XP')-rotated_dock.get_socket_location('XN')).length()
    check('상층 도크 회전 후 실제 면 중심',report['rotated_dock_cm']<=5)
    check('25m층 도크 환불',fresh.cancel_construction(3))
    for x in range(1,6):
        check('상층 수평 통로 '+str(x),fresh.reserve_module('Corridor',u.IntVector(x,3,1),0))
    check('25m층 90도 조선소 반대 포트',fresh.reserve_module('Yard',u.IntVector(8,0,1),1))
    upper=fresh.get_components_by_class(u.StaticMeshComponent)
    yard90=next(c for c in upper if 'YardSmall' in get(c,'static_mesh').get_name())
    links90=[c for c in upper if 'Straight' in get(c,'static_mesh').get_name()]
    report['rotated_yard_cm']=min((yard90.get_socket_location('YP')-c.get_socket_location('XP')).length() for c in links90)
    check('상층 조선소 회전 후 실제 면 중심',report['rotated_yard_cm']<=5)
    # 기존 1단계 흐름 뒤에 별도 시험으로 새 모듈과 실제 모델 충돌을 검사한다.
    hud.execute_ui_action('RestartScene')
    extended,=bases()
    check('조선소 측면 연결',extended.reserve_module('Corridor',u.IntVector(0,3,0),1) and extended.reserve_module('Yard',u.IntVector(-3,4,0),0))
    check('출입 여유·외곽 차단',not extended.reserve_module('Junction',u.IntVector(-4,5,0),0) and not extended.reserve_module('Junction',u.IntVector(-12,5,0),0))
    pc.action('Pause')
    pc.step_simulation(136)
    check('도크 수평 연결',extended.reserve_module('Corridor',u.IntVector(3,0,0),0) and extended.reserve_module('Dock',u.IntVector(4,-1,0),0))
    check('두 베이 무료 시험',extended.common_stock()==1200)
    parts=extended.get_components_by_class(u.StaticMeshComponent)
    yard,dock=part('YardSmall'),part('DockStandard')
    corridors=[c for c in parts if 'Straight' in get(c,'static_mesh').get_name()]
    measurements=[]
    for module,sock in [(yard,'YN'),(dock,'XN')]:
        point=module.get_socket_location(sock)
        other=min(corridors,key=lambda c:(c.get_socket_location('XP')-point).length())
        a,b=module.get_socket_rotation(sock),other.get_socket_rotation('XP')
        gap=(other.get_socket_location('XP')-point).length()
        n=u.MathLibrary.get_forward_vector(a).dot(u.MathLibrary.get_forward_vector(b))
        up=u.MathLibrary.get_up_vector(a).dot(u.MathLibrary.get_up_vector(b))
        measurements.append([get(module,'static_mesh').get_name(),gap,n,up])
        check(sock+' 신규 접합 중심·법선·롤',gap<5 and n<-.999 and up>.999)
    report['new_seams_cm_dot']=measurements
    ship,=get(pc,'ships')
    hull=next(c for c in ship.get_components_by_class(u.StaticMeshComponent) if get(c,'static_mesh') and get(c,'static_mesh').get_name()=='SM_PC14_FullMesh')
    bounds=get(hull,'static_mesh').get_bounding_box()
    scale=hull.get_world_scale()
    dims=(bounds.max-bounds.min)*scale*.5
    # 원본 메시 축에 관계없이 긴 축을 항진 방향에 정렬한 보수적 선체 상자.
    sizes=sorted([abs(dims.x),abs(dims.y),abs(dims.z)],reverse=True)
    half=u.Vector(*sizes)
    report['corvette_half_cm']=sizes
    check('원본 초계함 외곽',sizes[0]>100 and sizes[2]>0)
    for name,module,start,end,heading,wall in [
        ('조선소',yard,u.Vector(-12000,0,1570),u.Vector(-500,0,1570),0,u.Vector(-500,0,-900)),
        ('표준 도크',dock,u.Vector(0,9000,2240),u.Vector(0,0,2240),90,u.Vector(0,0,-900))]:
        origin=module.get_world_location()
        def sweep(a,b):
            return u.SystemLibrary.box_trace_single(world,origin+a,origin+b,half,u.Rotator(yaw=heading),u.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],u.DrawDebugTrace.NONE,False)
        hit=sweep(start,end)
        floor_hit=sweep(end,wall)
        report[name+'_sweep']={'blocked':hit is not None,'floor_detected':floor_hit is not None}
        check(name+' 초계함 외곽 진입·베이 바닥 검출',not hit and floor_hit is not None)
    pc.step_simulation(92)
    pc.action('Pause')
    def sample_frames(name):
        times=[]
        for _ in range(30): yield
        last=time.monotonic()
        for _ in range(120):
            yield
            now=time.monotonic()
            times.append((now-last)*1000)
            last=now
        times.sort()
        report.setdefault('editor_frame_interval_ms',{})[name]={'mean':round(sum(times)/len(times),2),'p95':round(times[113],2),'samples':len(times)}
    check('촬영 전 두 베이 완공',extended.remaining_seconds(4)==0)
    shotcam=get(pc,'range_camera')
    saved_loc,saved_rot=shotcam.get_actor_location(),shotcam.get_actor_rotation()
    u.SystemLibrary.execute_console_command(world,'ShowHUD',pc)
    old=pc.get_view_target()
    pc.set_view_target_with_blend(shotcam)
    for name,focus,offset in [
        ('Core',extended.get_actor_location(),u.Vector(13000,11000,7800)),
        ('YardBay',yard.get_world_location()+u.Vector(0,0,1600),u.Vector(-13100,-6200,4900)),
        ('DockBay',dock.get_world_location()+u.Vector(0,0,1600),u.Vector(-4300,15600,5800)),
        ('YardJoint',yard.get_socket_location('YN'),u.Vector(1700,-2300,1500)),
        ('DockJoint',dock.get_socket_location('XN'),u.Vector(-2300,1900,1400))]:
        frame(focus,offset)
        for _ in range(12): yield
        yield from capture(world,pc,name)
        if name in ('Core','YardBay'): yield from sample_frames(name)
    pc.set_view_target_with_blend(old)
    shotcam.set_actor_location_and_rotation(saved_loc,saved_rot,False,False)
    u.SystemLibrary.execute_console_command(world,'ShowHUD',pc)
    hud.execute_ui_action('PlaySceneExit')
    check('목록 복귀 시 모듈·함선 정리',not bases() and not get(pc,'ships'))
    check('단면 연출 시험 시작',pc.start_play_scenario('BaseTrial'))
    instance,=bases()
    check('단면 연출 창고 예약',instance.reserve_module('Corridor',u.IntVector(3,0,0),0) and instance.reserve_module('Storage',u.IntVector(4,0,0),0))
    store=next(c for c in instance.get_components_by_class(u.StaticMeshComponent) if 'StorageVault' in get(c,'static_mesh').get_name())
    focus=store.bounds.origin if hasattr(store,'bounds') else store.get_world_location()
    hud.execute_ui_action('BaseInspect')
    frame(focus,u.Vector(6900,6500,4100))
    yield from capture(world,pc,'Waiting')
    pc.action('Pause')
    pc.step_simulation(45)
    pc.action('Pause')
    check('창고 절반 시점',abs(instance.remaining_seconds(2)-30)<.01)
    yield from capture(world,pc,'Half')
    pc.action('Pause')
    pc.step_simulation(30)
    pc.action('Pause')
    yield from capture(world,pc,'Built')
    check('창고 완공',instance.remaining_seconds(2)==0)
    hud.execute_ui_action('PlaySceneExit')
    check('다른 임무 회귀',pc.start_play_scenario('Fleet50') and len(get(pc,'ships'))>1 and not bases())
    hud.execute_ui_action('BaseView')
    check('다른 임무에 시험장 생성 차단',not bases())

def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        report['passed']=True
    except Exception:
        report['passed']=False
        report['error']=traceback.format_exc()
    (folder/'PIE.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
