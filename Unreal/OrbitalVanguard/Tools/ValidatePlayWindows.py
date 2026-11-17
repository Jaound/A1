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
def hit_button(hud, action):
    center=hud.get_action_center(action)
    hit=str(hud.get_hit_action_at_canvas_point(center))
    check('WINDOW-UNIQUE-'+action.upper()+'-HIT',center.x>=0 and center.y>=0 and hit==action,
          {'center':(center.x,center.y),'hit':hit})

def observe_combat(pc):
    combat,wing,pd=p(pc,'combat'),p(pc,'carrier'),p(pc,'point_defense')
    return {'ships':snapshot(pc),
            'clocks':tuple(p(s,'game_time') for s in (combat,wing,pd)),
            'shots':tuple(p(s,'shot_count') for s in (combat,wing,pd)),
            'aircraft':[(p(a,'aircraft_id'),str(p(a,'state')),p(a,'hp'),p(a,'shield')) for a in p(wing,'aircraft')],
            'forced_mounts':[str(p(combat.get_ship_state(s),'forced_mount')) for s in p(pc,'ships')]}

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
            old_names=[p(s,'ship_name') for s in ships[:2]]
            for ship in ships[:2]:ship.set_editor_property('ship_name','같은 함선')
            first,second=(hud.get_fleet_display_label(s) for s in ships[:2])
            check('WINDOW-FLEET-DUPLICATE-NAMES-DISTINCT',first!=second and first.endswith('1') and second.endswith('2'),(first,second))
            for ship,name in zip(ships[:2],old_names):ship.set_editor_property('ship_name',name)
            check('WINDOW-FLEET-LABEL-READONLY',state['snapshot']==snapshot(pc))
            hud.set_details_open(False);pc.action('Pick_0')
            check('WINDOW-USER-PICK-OPENS',p(hud,'details_open') and p(pc,'selected')==ships[0])
            hud.execute_ui_action('WindowClose');pc.step_simulation(.1)
            check('WINDOW-CLOSE-STAYS-CLOSED',not p(hud,'details_open'))
            pc.action('Pick_0');check('WINDOW-RESELECT-REOPENS',p(hud,'details_open'))
            pc.action('Pick_1');check('WINDOW-NEW-SHIP',p(hud,'details_open') and p(pc,'selected')==ships[1])
            pc.select_ship_from_user(None,False);check('WINDOW-EMPTY-CLOSES',not p(hud,'details_open') and not p(pc,'selected'))
            pc.select_ship_from_user(ships[0],False);pc.select_ship_from_user(ships[1],True)
            check('WINDOW-MULTI-REPRESENTATIVE',p(hud,'details_open') and p(pc,'selected')==ships[0] and len(p(pc,'selection'))==2)
            pc.select_ship(ships[0])
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
            width,height=pc.get_viewport_size()
            scale=max(.25,min(1,width/1280,height/740))
            for action in ('Move','Stop','Target','FireAll','MoveAuto','FireAuto','Fire','NextWeapon','CancelAttack'):
                center=hud.get_action_center(action)
                check('WINDOW-COMMAND-CARD-'+action,
                      center.x<620 and center.y>height/scale-220
                      and str(hud.get_hit_action_at_canvas_point(center))==action
                      and hud.is_over_panel(center.x*scale,center.y*scale))
            check('WINDOW-BOTTOM-RIGHT-WORLD-AVAILABLE',not hud.is_over_panel(width-10,height-30))
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
            dev_pos=hud.get_development_window_position();dev_size=hud.get_development_window_size()
            x,y=dev_pos.x+30,dev_pos.y+12
            hud.handle_pointer(x,y,True,True,0.,False,0.,0.)
            hud.handle_pointer(x-50,y+20,False,True,0.,False,0.,0.)
            hud.handle_pointer(x-50,y+20,False,False,0.,False,0.,0.)
            moved=hud.get_development_window_position()
            check('WINDOW-DEV-TITLE-DRAGS',moved.x<dev_pos.x-45 and moved.y>dev_pos.y+15)
            x,y=moved.x+dev_size.x-5,moved.y+dev_size.y-5
            hud.handle_pointer(x,y,True,True,0.,False,0.,0.)
            hud.handle_pointer(x-50,y-30,False,True,0.,False,0.,0.)
            hud.handle_pointer(x-50,y-30,False,False,0.,False,0.,0.)
            resized=hud.get_development_window_size()
            check('WINDOW-DEV-CORNER-RESIZES',resized.x<dev_size.x-45 and resized.y<dev_size.y-25)
            check('WINDOW-DEV-FRAME-READONLY',snapshot(pc)==before_simulation)
            state.update(phase=81,at=time.monotonic(),dev_snapshot=before_simulation);return
        if state['phase']==81:
            before_simulation=state['dev_snapshot']
            panel=hud.get_development_panel();before_native=hud.get_development_window_position()
            check('WINDOW-DEV-NATIVE-DRAG-DISPATCH',panel.debug_frame_gesture(False,unreal.Vector2D(30,10)))
            native_pos=hud.get_development_window_position()
            check('WINDOW-DEV-NATIVE-DRAG-MOVES',native_pos.x>before_native.x+25 and native_pos.y>before_native.y+5)
            before_native_size=hud.get_development_window_size()
            check('WINDOW-DEV-NATIVE-RESIZE-DISPATCH',panel.debug_frame_gesture(True,unreal.Vector2D(-25,-20)))
            after_native_size=hud.get_development_window_size()
            check('WINDOW-DEV-NATIVE-RESIZE-CHANGES',after_native_size.x<before_native_size.x-20 and after_native_size.y<before_native_size.y-15,(before_native_size,after_native_size))
            check('WINDOW-DEV-NATIVE-READONLY',snapshot(pc)==before_simulation)
            saved_pos=hud.get_development_window_position();saved_size=hud.get_development_window_size()
            hud.toggle_development_minimized()
            check('WINDOW-DEV-MINIMIZES',p(hud,'development_minimized') and hud.get_development_window_size().y<80)
            check('WINDOW-DEV-MIN-DRAG-DISPATCH',panel.debug_frame_gesture(False,unreal.Vector2D(10,0)))
            hud.toggle_development_minimized()
            check('WINDOW-DEV-MIN-RESTORES',not p(hud,'development_minimized') and (hud.get_development_window_size()-saved_size).length()<.1)
            saved_pos=hud.get_development_window_position()
            hud.toggle_development_maximized()
            check('WINDOW-DEV-MAXIMIZES',hud.get_development_window_size().x>saved_size.x)
            hud.toggle_development_maximized()
            check('WINDOW-DEV-MAX-RESTORES',(hud.get_development_window_size()-saved_size).length()<.1 and (hud.get_development_window_position()-saved_pos).length()<.1)
            hud.reset_development_window()
            check('WINDOW-DEV-RESET',hud.get_development_window_size().x>saved_size.x and snapshot(pc)==before_simulation)
            pc.action('Developer')
            check('WINDOW-DEV-OVERLAY-PREVIEW-RESTORED',hud.has_orbit_preview())
            # 정상 임무를 시작한 뒤 정지한다. 아래 검사는 출격·귀환 명령 없이
            # 두 화면의 고유 버튼과 조회가 실제 상태를 바꾸지 않는지 확인한다.
            pc.action('MissionAttack')
            if not p(pc,'simulation_paused'):pc.action('Pause')
            pc.set_editor_property('developer_mode',False)
            hud.set_hero_panel_open(False);hud.set_details_open(False)
            hud.execute_ui_action('InteriorClose')
            state.update(phase=9,at=time.monotonic(),mission_observation=observe_combat(pc));return
        if state['phase']==9:
            check('WINDOW-UNIQUE-NORMAL-MISSION-FIXTURE',p(p(pc,'mission'),'enabled') and
                  p(pc,'simulation_paused') and not p(pc,'developer_mode'))
            # 같은 명령을 수행해도 표시 위치마다 별도 HitBox가 있어야 한다.
            for action in ('HeroPanel','MissionHeroPanel','CommandHeroPanel'):
                hit_button(hud,action)
                hud.execute_ui_action(action)
                check('WINDOW-UNIQUE-'+action.upper()+'-OPENS',hud.is_hero_panel_open())
                hud.execute_ui_action(action)
                check('WINDOW-UNIQUE-'+action.upper()+'-CLOSES',not hud.is_hero_panel_open())
            hud.execute_ui_action('SceneMenu')
            state.update(phase=10,at=time.monotonic());return
        if state['phase']==10:
            hit_button(hud,'SceneMenu');hit_button(hud,'SceneMenuClose')
            hud.execute_ui_action('SceneMenuClose')
            wing=p(pc,'carrier')
            carriers=[p(c,'carrier') for c in p(wing,'carriers') if p(c,'carrier') and
                      p(p(p(c,'carrier'),'systems'),'faction_id')==0]
            check('WINDOW-UNIQUE-REGISTERED-CARRIER-FIXTURE',bool(carriers))
            pc.select_ship(carriers[0]);hud.execute_ui_action('External');hud.execute_ui_action('HangarView')
            state.update(phase=11,at=time.monotonic());return
        if state['phase']==11:
            check('WINDOW-UNIQUE-SCENE-MENU-CLOSED',hud.get_action_center('SceneMenuClose').x<0)
            for action in ('HangarLaunchAircraft','HangarRecallAircraft','LaunchAircraft','RecallAircraft'):
                hit_button(hud,action)
            records=[a for a in p(p(pc,'carrier'),'aircraft') if p(a,'carrier')==p(pc,'selected') and p(a,'faction_id')==0]
            check('WINDOW-AIRCRAFT-CARD-FIXTURE',bool(records))
            craft_id=p(records[0],'aircraft_id');card='AircraftCard_'+str(craft_id)
            state['first_aircraft_id']=craft_id
            hit_button(hud,card);hud.execute_ui_action(card)
            check('WINDOW-AIRCRAFT-CARD-SELECTS-ID',p(hud,'selected_aircraft_id')==craft_id)
            check('WINDOW-AIRCRAFT-CARD-READONLY',observe_combat(pc)==state['mission_observation'])
            hit_button(hud,'AirScope');before_scope=hud.is_fleet_air_overview()
            hud.execute_ui_action('AirScope')
            check('WINDOW-AIR-SCOPE-CHANGES',hud.is_fleet_air_overview()!=before_scope)
            hud.execute_ui_action('AirScope')
            check('WINDOW-AIR-SCOPE-RESTORES',hud.is_fleet_air_overview()==before_scope)
            check('WINDOW-AIR-SCOPE-READONLY',observe_combat(pc)==state['mission_observation'])
            check('WINDOW-UNIQUE-HANGAR-OBSERVATION-NO-MUTATION',observe_combat(pc)==state['mission_observation'])
            check('WINDOW-AIRCRAFT-PAGE-FIXTURE',len(records)>15)
            state['second_aircraft_id']=p(records[1],'aircraft_id')
            state['next_aircraft_id']=p(records[15],'aircraft_id')
            hud.execute_ui_action('AircraftNext')
            state.update(phase=111,at=time.monotonic());return
        if state['phase']==111:
            card='AircraftCard_'+str(state['next_aircraft_id'])
            hit_button(hud,card);hud.execute_ui_action(card)
            check('WINDOW-AIRCRAFT-PAGE-SELECTS-STABLE-ID',p(hud,'selected_aircraft_id')==state['next_aircraft_id'])
            chosen=next(a for a in p(p(pc,'carrier'),'aircraft') if p(a,'aircraft_id')==state['next_aircraft_id'])
            expected=p(pc,'selected').get_actor_location() if p(chosen,'state')==unreal.OVAircraftState.STORED else p(chosen,'position')
            hud.execute_ui_action('AircraftLocate')
            check('WINDOW-AIRCRAFT-LOCATE-ACTUAL', (p(pc,'camera_focus')-expected).length()<.1 and not p(pc,'follow'))
            check('WINDOW-AIRCRAFT-PAGE-READONLY',observe_combat(pc)==state['mission_observation'])
            for _ in range(3):hud.execute_ui_action('AircraftFilter')
            state.update(phase=112,at=time.monotonic());return
        if state['phase']==112:
            check('WINDOW-AIRCRAFT-LOSS-FILTER-EMPTY',hud.get_action_center('AircraftCard_'+str(state['next_aircraft_id'])).x<0)
            check('WINDOW-AIRCRAFT-FILTER-READONLY',observe_combat(pc)==state['mission_observation'])
            wing=p(pc,'carrier')
            check('WINDOW-AIRCRAFT-DAMAGE-FIXTURE',wing.apply_aircraft_damage(state['first_aircraft_id'],10))
            check('WINDOW-AIRCRAFT-LOSS-FIXTURE',wing.apply_aircraft_damage(state['second_aircraft_id'],10000))
            state['mission_observation']=observe_combat(pc)
            hud.execute_ui_action('AircraftFilter')
            state.update(phase=113,at=time.monotonic());return
        if state['phase']==113:
            check('WINDOW-AIRCRAFT-FILTER-RESTORES',hud.get_action_center('AircraftCard_'+str(state['first_aircraft_id'])).x>=0)
            hud.execute_ui_action('AircraftFilter');hud.execute_ui_action('AircraftFilter')
            state.update(phase=114,at=time.monotonic());return
        if state['phase']==114:
            check('WINDOW-AIRCRAFT-DAMAGED-INCLUDED',hud.get_action_center('AircraftCard_'+str(state['first_aircraft_id'])).x>=0)
            check('WINDOW-AIRCRAFT-DESTROYED-NOT-DAMAGED',hud.get_action_center('AircraftCard_'+str(state['second_aircraft_id'])).x<0)
            hud.execute_ui_action('AircraftFilter')
            state.update(phase=115,at=time.monotonic());return
        if state['phase']==115:
            check('WINDOW-AIRCRAFT-LOSS-INCLUDED',hud.get_action_center('AircraftCard_'+str(state['second_aircraft_id'])).x>=0)
            check('WINDOW-AIRCRAFT-DAMAGED-NOT-LOSS',hud.get_action_center('AircraftCard_'+str(state['first_aircraft_id'])).x<0)
            check('WINDOW-AIRCRAFT-FILTER-AFTER-DAMAGE-READONLY',observe_combat(pc)==state['mission_observation'])
            hud.execute_ui_action('AircraftFilter')
            hud.execute_ui_action('HangarDisplayMode')
            state.update(phase=116,at=time.monotonic());return
        if state['phase']==116:
            hit_button(hud,'HangarDisplayMode')
            check('WINDOW-HANGAR-FACILITY-VIEW',hud.get_action_center('AircraftCard_'+str(state['first_aircraft_id'])).x<0)
            check('WINDOW-HANGAR-FACILITY-READONLY',observe_combat(pc)==state['mission_observation'])
            hud.execute_ui_action('HangarDisplayMode')
            hud.execute_ui_action('Deck1')
            pc.set_editor_property('camera_focus',unreal.Vector(987,654,321));hud.execute_ui_action('AircraftLocate')
            check('WINDOW-AIRCRAFT-LOCATE-HIDDEN-BLOCKED',(p(pc,'camera_focus')-unreal.Vector(987,654,321)).length()<.1)
            state.update(phase=12,at=time.monotonic());return
        if state['phase']==12:
            for action in ('DetailsInteriorToggle','InteriorToggle'):
                hit_button(hud,action)
            hud.execute_ui_action('DetailsInteriorToggle')
            check('WINDOW-UNIQUE-DETAILS-INTERIOR-OPENS',p(hud,'interior_open'))
            hud.execute_ui_action('InteriorToggle')
            check('WINDOW-UNIQUE-BASE-INTERIOR-CLOSES',not p(hud,'interior_open'))
            check('WINDOW-UNIQUE-UI-NO-SHOTS-OR-INVENTORY-MUTATION',observe_combat(pc)==state['mission_observation'])
            hud.execute_ui_action('InteriorToggle');hud.execute_ui_action('InteriorClear')
            state.update(phase=13,at=time.monotonic());return
        if state['phase']==13:
            # 목록과 지도는 같은 개인을 제어하지만 서로 다른 HitBox를 사용한다.
            # 여러 인물이 겹칠 수 있으므로 실제 최상단에 노출된 마커를 선택한다.
            visible=[]
            for unit in p(p(pc,'interior'),'units'):
                unit_id=p(unit,'unit_id');action='InteriorMarker_'+str(unit_id)
                center=hud.get_action_center(action)
                if center.x>=0 and center.y>=0:
                    visible.append((unit_id,action,center,str(hud.get_hit_action_at_canvas_point(center))))
            check('WINDOW-UNIQUE-INTERIOR-MARKERS-REGISTERED',bool(visible),len(visible))
            exposed=next((entry for entry in visible if entry[1]==entry[3]),None)
            check('WINDOW-UNIQUE-INTERIOR-MARKER-CENTER-HIT',exposed is not None,
                  [(entry[1],entry[3]) for entry in visible])
            unit_id,action,center,hit=exposed
            hud.execute_ui_action(hit)
            check('WINDOW-UNIQUE-INTERIOR-MARKER-SELECTS-REAL-UNIT',list(p(hud,'interior_selected_units'))==[unit_id])
            hud.execute_ui_action(hit)
            check('WINDOW-UNIQUE-INTERIOR-MARKER-DESELECTS-REAL-UNIT',not list(p(hud,'interior_selected_units')))
            check('WINDOW-UNIQUE-INTERIOR-MARKER-NO-SIMULATION',observe_combat(pc)==state['mission_observation'])
            hit_button(hud,'InteriorHeroPanel')
            hud.execute_ui_action('InteriorHeroPanel')
            check('WINDOW-INTERIOR-HERO-OPENS',hud.is_hero_panel_open())
            hud.execute_ui_action('InteriorHeroPanel')
            check('WINDOW-INTERIOR-HERO-CLOSES',not hud.is_hero_panel_open())
            check('WINDOW-INTERIOR-HERO-NO-SIMULATION',observe_combat(pc)==state['mission_observation'])
            width,height=pc.get_viewport_size();scale=max(.25,min(1,width/1280,height/740))
            pos=hud.get_interior_window_position();size=hud.get_interior_window_size()
            x,y=(pos.x+30)*scale,(pos.y+12)*scale
            hud.handle_pointer(x,y,True,True,0.,False,0.,0.)
            hud.handle_pointer(x-50*scale,y+15*scale,False,True,0.,False,0.,0.)
            hud.handle_pointer(x-50*scale,y+15*scale,False,False,0.,False,0.,0.)
            moved=hud.get_interior_window_position()
            check('WINDOW-INTERIOR-DRAG-MOVES',moved.x<pos.x-45 and moved.y>pos.y+10)
            x,y=(moved.x+size.x-4)*scale,(moved.y+size.y-4)*scale
            hud.handle_pointer(x,y,True,True,0.,False,0.,0.)
            hud.handle_pointer(x-40*scale,y-30*scale,False,True,0.,False,0.,0.)
            hud.handle_pointer(x-40*scale,y-30*scale,False,False,0.,False,0.,0.)
            resized=hud.get_interior_window_size()
            check('WINDOW-INTERIOR-RESIZE-CHANGES',resized.x<size.x-35 and resized.y<size.y-25)
            state.update(phase=14,at=time.monotonic(),interior_unit_id=unit_id);return
        if state['phase']==14:
            action='InteriorMarker_'+str(state['interior_unit_id']);center=hud.get_action_center(action)
            check('WINDOW-INTERIOR-MOVED-MARKER-HIT',str(hud.get_hit_action_at_canvas_point(center))==action)
            hud.execute_ui_action(action)
            check('WINDOW-INTERIOR-MOVED-MARKER-SELECTS',state['interior_unit_id'] in list(p(hud,'interior_selected_units')))
            check('WINDOW-INTERIOR-MOVE-RESIZE-READONLY',observe_combat(pc)==state['mission_observation'])
            original_pos=hud.get_interior_window_position();original_size=hud.get_interior_window_size()
            hud.execute_ui_action('InteriorMin')
            check('WINDOW-INTERIOR-MINIMIZES',p(hud,'interior_minimized') and abs(hud.get_interior_window_size().y-32)<.1)
            hud.execute_ui_action('InteriorMin')
            check('WINDOW-INTERIOR-MIN-RESTORES',not p(hud,'interior_minimized') and (hud.get_interior_window_size()-original_size).length()<.1)
            hud.execute_ui_action('InteriorMax')
            check('WINDOW-INTERIOR-MAXIMIZES',hud.get_interior_window_size().x>original_size.x)
            hud.execute_ui_action('InteriorMax')
            check('WINDOW-INTERIOR-MAX-RESTORES',(hud.get_interior_window_size()-original_size).length()<.1 and (hud.get_interior_window_position()-original_pos).length()<.1)
            width,height=pc.get_viewport_size();scale=max(.25,min(1,width/1280,height/740))
            pos=hud.get_interior_window_position();x,y=(pos.x+30)*scale,(pos.y+12)*scale
            hud.handle_pointer(x,y,True,True,0.,False,0.,0.);hud.execute_ui_action('InteriorClose')
            hud.handle_pointer(x-30*scale,y,False,True,0.,False,0.,0.)
            check('WINDOW-INTERIOR-CLOSE-CANCELS-DRAG',hud.get_interior_window_position()==pos)
            finish()
    except Exception:
        report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1]);finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
