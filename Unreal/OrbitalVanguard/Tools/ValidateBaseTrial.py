"""ControlRange의 실제 PIE 임무 수명·시간·소켓을 검사한다. 마우스 검사는 별도로 수행한다."""
import json
import traceback
from pathlib import Path
import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
folder=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))/'Validation/BaseTrial'
folder.mkdir(parents=True,exist_ok=True)
report={'checks':[]}
def check(label, condition):
    if not condition: raise AssertionError(label)
    report['checks'].append(label)

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
    view=pc.get_view_target()
    check('기지 카메라',view!=get(pc,'range_camera'))
    def reserve(name,x,y,z,turn=0):
        return base.reserve_module(name,u.IntVector(x,y,z),turn)
    check('겹침 차단',not reserve('Storage',0,0,0))
    check('방향 차단',not reserve('Corridor',3,0,0,1))
    check('공중 고립 차단',not reserve('Corridor',3,0,1))
    check('코어 통로 창고 예약',reserve('Corridor',3,0,0) and reserve('Storage',4,0,0))
    check('종속 공사 보호',not base.cancel_construction(1))
    check('별도 연결 슬롯',reserve('Junction',-3,0,0) and base.common_stock()==820)
    # 렌더 파생 이미지와 배치 검사는 별개다. 일시정지 상태에서 셰이더와 카메라가 적용될 프레임을 준다.
    for _ in range(30): yield
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
    check('미완공 허브 취소 전액',base.cancel_construction(3) and base.common_stock()==860)
    pc.step_simulation(20)
    check('일시정지 공사 동결',base.remaining_seconds(1)==15 and base.remaining_seconds(2)==60)
    hud.execute_ui_action('BaseBack')
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
    hud.execute_ui_action('PlaySceneExit')
    check('목록 복귀 시 모듈·함선 정리',not bases() and not get(pc,'ships'))
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
