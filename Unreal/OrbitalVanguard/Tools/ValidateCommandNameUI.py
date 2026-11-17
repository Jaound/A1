"""명령 카드 긴 이름 UI의 별도 PIE/API 검사. 실제 입력 검사가 아니다."""
import json, os, time, traceback
import unreal
saved=unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
width,height=(int(x) for x in os.environ.get('OV_UI_CAPTURE_RES','1280x720').split('x'))
out=os.path.join(saved,'Validation','CommandNameUI_%dx%d.json'%(width,height))
png=os.path.join(saved,'Validation','CommandNameUI_%dx%d.png'%(width,height)).replace('\\','/')
os.makedirs(os.path.dirname(out),exist_ok=True)
report={'passed':None,'physical_input':False,'resolution':[width,height],'checks':[],'errors':[],'image':png}
state={'phase':'start','started':time.monotonic()}
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
def p(o,k): return o.get_editor_property(k)
def check(key,ok,detail=''):
    report['checks'].append({'id':key,'passed':bool(ok),'detail':str(detail)})
    if not ok: raise AssertionError(key+': '+str(detail))
def finish():
    if state.get('floating'): report['play_settings_restored']=bool(unreal.OVDevelopmentEditorLibrary.restore_preview_settings())
    report['passed']=bool(report['checks']) and all(c['passed'] for c in report['checks']) and not report['errors']
    with open(out,'w',encoding='utf-8') as f: json.dump(report,f,ensure_ascii=False,indent=2)
    unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(_):
    try:
        if time.monotonic()-state['started']>90: raise RuntimeError('명령 카드 검사 시간 초과')
        if state['phase']=='start':
            if os.path.isfile(png): os.remove(png)
            check('WINDOW_START',unreal.OVDevelopmentEditorLibrary.start_preview_window(width,height))
            state.update(phase='wait',floating=True);return
        world=editor.get_game_world();pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
        if not pc or len(p(pc,'ships'))<3:return
        if state['phase']=='wait':
            pc.start_combat_range();ships=list(p(pc,'ships')); source,target=ships[1],ships[2]
            source.set_editor_property('ship_name','아주 긴 이름을 가진 순양함 알파 베타 감마 델타 '*4)
            target.set_editor_property('ship_name','매우 긴 이름을 가진 표적 함선 하나 둘 셋 넷 다섯 여섯 '*4)
            pc.select_ship(source);check('TARGET',p(pc,'combat').set_target(source,target))
            hud=pc.get_hud();hud.set_details_open(False);hud.set_hero_panel_open(False)
            state.update(phase='labels',at=time.monotonic(),source=source,target=target);return
        hud=pc.get_hud(); source=state['source'];target=state['target']
        if state['phase']=='labels' and time.monotonic()-state['at']>1:
            a=hud.get_action_center('CommandShipLabel');b=hud.get_action_center('CommandTargetLabel')
            check('SHIP_TOOLTIP',hud.get_command_title_tooltip(a)==p(source,'ship_name'),(str(a),str(hud.get_hit_action_at_canvas_point(a)),hud.get_command_title_tooltip(a),p(source,'ship_name')))
            check('TARGET_TOOLTIP',hud.get_command_title_tooltip(b)=='표적: '+p(target,'ship_name'),(str(b),str(hud.get_hit_action_at_canvas_point(b)),hud.get_command_title_tooltip(b),p(target,'ship_name')))
            check('EMPTY_POINT',hud.get_command_title_tooltip(unreal.Vector2D(-10,-10))=='')
            before=p(pc,'selected');hud.execute_ui_action('CommandShipLabel');hud.execute_ui_action('CommandTargetLabel')
            check('LABELS_DO_NOT_CHANGE_SELECTION',p(pc,'selected')==before)
            source.set_editor_property('ship_name','짧음');target.set_editor_property('ship_name','표적')
            state.update(phase='short',at=time.monotonic());return
        if state['phase']=='short' and time.monotonic()-state['at']>.5:
            a=hud.get_action_center('CommandShipLabel');b=hud.get_action_center('CommandTargetLabel')
            check('SHORT_SHIP_NO_TOOLTIP',hud.get_command_title_tooltip(a)=='')
            check('SHORT_TARGET_NO_TOOLTIP',hud.get_command_title_tooltip(b)=='')
            source.set_editor_property('ship_name','아주 긴 이름을 가진 순양함 알파 베타 감마 델타 '*4)
            target.set_editor_property('ship_name','매우 긴 이름을 가진 표적 함선 하나 둘 셋 넷 다섯 여섯 '*4)
            state.update(phase='warm',at=time.monotonic());return
        if state['phase']=='warm' and time.monotonic()-state['at']>1:
            unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+png+'" -nosuffix',pc)
            state.update(phase='capture',at=time.monotonic());return
        if state['phase']=='capture' and time.monotonic()-state['at']>2:
            check('RENDER_FILE',os.path.isfile(png));finish()
    except Exception:
        report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
