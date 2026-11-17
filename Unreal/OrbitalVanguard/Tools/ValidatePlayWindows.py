"""플레이 상세창·핀·프리뷰 API 회귀. 직접 입력·조작감 판정과 분리한다."""
import json
import os
import time
import traceback
import unreal

out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Validation', 'PlayWindows.json')
report = {'passed': None, 'checks': [], 'errors': [], 'physical_input': False}
state = {'phase': 0, 'at': time.monotonic(), 'started': time.monotonic()}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

def p(o, key): return o.get_editor_property(key)
def check(key, value, detail=''):
    report['checks'].append({'id': key, 'passed': bool(value), 'detail': str(detail)})
    if not value: raise AssertionError(key + ': ' + str(detail))
def snapshot(pc):
    return [(s.get_path_name(), p(p(s, 'systems'), 'hull_hp'), p(p(s, 'systems'), 'shield'), len(s.get_weapon_visual_states())) for s in p(pc, 'ships')]
def finish():
    report['passed'] = not report['errors'] and bool(report['checks'])
    with open(out, 'w', encoding='utf-8') as f: json.dump(report, f, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(_dt):
    try:
        if time.monotonic()-state['started']>90: raise RuntimeError('창 검사 시간 초과')
        if state['phase']==0:
            level.editor_request_begin_play(); state['phase']=1; return
        world=editor.get_game_world()
        pc=unreal.GameplayStatics.get_player_controller(world,0) if world else None
        if not pc or len(p(pc,'ships'))<3:return
        hud=pc.get_hud()
        if state['phase']==1:
            pc.start_calibration_range(1); pc.set_editor_property('developer_mode',False)
            if not p(pc,'simulation_paused'):pc.action('Pause')
            hud.set_hero_panel_open(False);hud.execute_ui_action('External')
            state.update(phase=2,at=time.monotonic(),snapshot=snapshot(pc));return
        if time.monotonic()-state['at']<.5:return
        if state['phase']==2:
            check('WINDOW-MESH-MIRROR',hud.get_preview_mesh_count()>0)
            check('WINDOW-MIRROR-NO-DAMAGE',state['snapshot']==snapshot(pc))
            ships=list(p(pc,'ships'))
            for i in range(3):
                pc.select_ship(ships[i]);hud.execute_ui_action('WindowPin')
                check('WINDOW-PIN-LIMIT-'+str(i),hud.get_pinned_count()==min(i+1,2))
            pc.select_ship(ships[0]);hud.execute_ui_action('WindowPin');check('WINDOW-UNPIN',hud.get_pinned_count()==1)
            for _ in range(40):
                hud.set_hero_panel_open(True);hud.set_hero_panel_open(False)
                hud.execute_ui_action('External');hud.execute_ui_action('WindowMin');hud.execute_ui_action('WindowMin')
            check('WINDOW-REFRESH-NO-SIMULATION',state['snapshot']==snapshot(pc))
            check('WINDOW-REAL-SHIP-COUNT',len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.OVShip))==3)
            state.update(phase=3,at=time.monotonic());return
        if state['phase']==3:
            point=p(hud,'window_position')
            center=unreal.Vector2D(point.x+2,point.y+120)
            check('WINDOW-EMPTY-GUTTER-CONSUMES',str(hud.get_hit_action_at_canvas_point(center))=='WindowBackground')
            check('WINDOW-PAUSED-CLOCK',p(p(pc,'combat'),'game_time')==0)
            pc.start_combat_range();hud.execute_ui_action('External')
            state.update(phase=4,at=time.monotonic());return
        if state['phase']==4:
            check('WINDOW-SCENE-SWITCH-MIRROR',hud.get_preview_mesh_count()>0)
            check('WINDOW-SCENE-NO-SHIP-CLONE',len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.OVShip))==3)
            # Existing pinned actor is one of the reused baseline ships. Remove it explicitly,
            # then exercise two simultaneous pins and information loss on departure.
            ships=list(p(pc,'ships'));pc.select_ship(ships[1]);hud.execute_ui_action('WindowPin')
            check('WINDOW-PIN-RESET-FIXTURE',hud.get_pinned_count()==0)
            pc.start_calibration_range(1)
            ships=list(p(pc,'ships'));attacker,target=ships[0],ships[2]
            combat=p(pc,'combat');pc.select_ship(attacker);pc.set_target(target)
            # Explicit DEV presence fixture, not a claim that troops entered through UI.
            p(target,'systems').set_interior_presence(0,True,False,True)
            combat.set_protection_policy(attacker,unreal.OVProtectionPolicy.UNTIL_EXIT)
            hud.set_details_open(False);hud.set_hero_panel_open(False)
            state.update(phase=5,at=time.monotonic(),attacker=attacker,target=target,
                         shots=p(combat,'shot_count'));return
        if state['phase']==5:
            combat=p(pc,'combat');attacker=state['attacker'];target=state['target']
            for action in ('Protection','ForceGun'):
                center=hud.get_action_center(action)
                check('WINDOW-'+action.upper()+'-BUTTON-VISIBLE',center.x>=0 and center.y>=0,center)
                check('WINDOW-'+action.upper()+'-BUTTON-HIT',str(hud.get_hit_action_at_canvas_point(center))==action)
            before=p(combat.get_ship_state(attacker),'protection')
            hud.execute_ui_action('Protection')
            check('WINDOW-PROTECTION-UI-CHANGES-POLICY',p(combat.get_ship_state(attacker),'protection')!=before)
            hud.execute_ui_action('Protection');hud.execute_ui_action('Protection')
            check('WINDOW-PROTECTION-CYCLE-RETURNS',p(combat.get_ship_state(attacker),'protection')==before)
            hud.execute_ui_action('ForceGun')
            order=combat.get_ship_state(attacker)
            forced=str(p(order,'forced_mount'))
            requested=[str(p(m,'mount_id')) for m in p(order,'mounts') if p(m,'fire_requested')]
            check('WINDOW-FORCED-UI-SELECTED-MOUNT',forced!='None' and requested==[forced],requested)
            check('WINDOW-FORCED-UI-NO-PAUSED-SHOT',p(combat,'shot_count')==state['shots'])
            hud.execute_ui_action('Stop')
            check('WINDOW-FORCED-UI-CANCEL-RECOVERY',str(p(combat.get_ship_state(attacker),'forced_mount'))=='None')
            for ship in (attacker,target):
                pc.select_ship(ship);hud.execute_ui_action('WindowPin')
            check('WINDOW-TWO-SIMULTANEOUS-PINS',hud.get_pinned_count()==2)
            hud.set_details_open(False);combat.notify_ftl_departure(target)
            state.update(phase=6,at=time.monotonic());return
        if state['phase']==6:
            check('WINDOW-FTL-PIN-REMOVED',hud.get_pinned_count()==1,hud.get_pinned_count())
            state['attacker'].set_actor_hidden_in_game(True)
            state.update(phase=7,at=time.monotonic());return
        if state['phase']==7:
            check('WINDOW-HIDDEN-PIN-REMOVED',hud.get_pinned_count()==0,hud.get_pinned_count())
            state['attacker'].set_actor_hidden_in_game(False)
            check('WINDOW-PINS-NO-SHIP-CLONES',len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.OVShip))==3)
            pc.select_ship(state['attacker']);hud.set_hero_panel_open(False)
            hud.execute_ui_action('External')
            # Wait for the visible window to clamp to the viewport before taking snapshots.
            state.update(phase=8,at=time.monotonic());return
        if state['phase']==8:
            check('WINDOW-DEV-OVERLAY-VISIBLE-PREVIEW-FIXTURE',hud.has_orbit_preview())
            rotation=hud.get_preview_rotation()
            before_rotation=tuple(float(getattr(rotation,key)) for key in ('pitch','yaw','roll'))
            before_zoom=float(hud.get_preview_zoom())
            position=p(hud,'window_position');before_position=(position.x,position.y)
            before_simulation=snapshot(pc)
            width,height=pc.get_viewport_size()
            check('WINDOW-DEV-OVERLAY-VIEWPORT-FIXTURE',width>0 and height>0,(width,height))
            scale=max(.25,min(1.,width/1280.,height/740.))
            pc.action('Developer')
            check('WINDOW-DEV-OVERLAY-NO-ORBIT-PREVIEW',not hud.has_orbit_preview())
            # API pointer samples target the old, now hidden title bar and preview area.
            # These are not OS mouse events; no camera or simulation clock is advanced.
            title_x,title_y=(position.x+20)*scale,(position.y+10)*scale
            hud.handle_pointer(title_x,title_y,True,True,0.,False,0.,0.)
            hud.handle_pointer(title_x+60*scale,title_y+30*scale,False,True,0.,False,0.,0.)
            preview_x,preview_y=(position.x+160)*scale,(position.y+150)*scale
            hud.handle_pointer(preview_x,preview_y,False,False,1.,True,36.,22.)
            hud.handle_pointer(preview_x,preview_y,False,False,0.,False,0.,0.)
            after_rotation=hud.get_preview_rotation()
            check('WINDOW-DEV-OVERLAY-ROTATION-UNCHANGED',
                  tuple(float(getattr(after_rotation,key)) for key in ('pitch','yaw','roll'))==before_rotation)
            check('WINDOW-DEV-OVERLAY-ZOOM-UNCHANGED',float(hud.get_preview_zoom())==before_zoom)
            after_position=p(hud,'window_position')
            check('WINDOW-DEV-OVERLAY-POSITION-UNCHANGED',(after_position.x,after_position.y)==before_position)
            check('WINDOW-DEV-OVERLAY-NO-SIMULATION',snapshot(pc)==before_simulation)
            pc.action('Developer')
            check('WINDOW-DEV-OVERLAY-PREVIEW-RESTORED',hud.has_orbit_preview())
            finish()
    except Exception:
        report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
