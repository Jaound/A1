"""실제 PIE 렌더 화면·읽기전용 프리뷰 검사. OS 직접 입력 검증이 아니다."""
import json
import os
import time
import traceback
import unreal

stamp = time.strftime('%Y%m%d_%H%M%S')
out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), 'Validation', 'PlayUI', stamp)
os.makedirs(out, exist_ok=False)
size = os.environ.get('OV_UI_CAPTURE_RES', '1280x720')
width, height = map(int, size.split('x'))
report = {'passed': None, 'kind': 'PIE 렌더와 UI API', 'physical_input': False, 'requested_size': [width, height], 'captures': [], 'checks': [], 'errors': []}
state = {'stage': 'start', 'at': time.monotonic(), 'started': time.monotonic(), 'index': 0}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
views = ['전투_HUD', '영웅_카드', '외부_무장', '핀_비교', '핀_무장', '핀_시설', '격납고', '선내_영웅', '개발_함선', '개발_영웅', '개발_무장', '개발_시험', '초계함_모델', '개발_접기', '개발_확대', '전투_알림', '쉴드_고갈_알림', '적_쉴드_알림', '항공대_위기', '항공대_필터']
if os.environ.get('OV_UI_ONLY_PIN') == '1': views = ['핀_비교']
if os.environ.get('OV_UI_TOOLTIP_TEST') == 'fleet': views = ['함대_긴이름']
if os.environ.get('OV_UI_ONLY_PIN_HANGAR') == '1': views = ['항공대_핀']

def prop(obj, key):
    return obj.get_editor_property(key)

def check(identifier, value, detail=''):
    report['checks'].append({'id': identifier, 'passed': bool(value), 'detail': str(detail)})
    if not value:
        raise AssertionError(identifier + ': ' + str(detail))

def snapshot(pc):
    return [(s.get_path_name(), prop(prop(s, 'systems'), 'hull_hp'), prop(prop(s, 'systems'), 'shield')) for s in prop(pc, 'ships')]

def show_heroes(hud, value):
    # Stable action used by the actual button; compatible with the first UI build.
    visible = hud.get_hero_panel().get_visibility() == unreal.SlateVisibility.VISIBLE
    if visible != value:
        hud.execute_ui_action('HeroPanel')

def finish():
    try:
        report['preview_settings_restored'] = bool(unreal.OVDevelopmentEditorLibrary.restore_preview_settings())
    except Exception:
        report['errors'].append(traceback.format_exc())
    report['passed'] = not report['errors'] and len(report['captures']) == len(views)
    report['elapsed_seconds'] = time.monotonic() - state['started']
    with open(os.path.join(out, 'report.json'), 'w', encoding='utf-8') as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state['handle'])

def tick(_dt):
    try:
        if time.monotonic() - state['started'] > 150:
            raise RuntimeError('PIE 캡처 제한 시간 초과')
        if state['stage'] == 'start':
            check('PLAY-WINDOW', unreal.OVDevelopmentEditorLibrary.start_preview_window(width, height))
            state['stage'] = 'world'
            return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(prop(pc, 'ships')) < 3:
            return
        hud = pc.get_hud()
        if state['stage'] == 'world':
            pc.action('MissionAttack')
            if not prop(pc, 'simulation_paused'):
                pc.action('Pause')
            if prop(pc, 'developer_mode'):
                pc.action('Developer')
            state.update(stage='prepare', at=time.monotonic())
            return
        if time.monotonic() - state['at'] < .8:
            return
        name = views[state['index']]
        if state['stage'] == 'prepare':
            show_heroes(hud, False)
            hud.set_details_open(False)
            hud.execute_ui_action('InteriorClose')
            if prop(pc, 'developer_mode'):
                pc.action('Developer')
            if name == '영웅_카드':
                show_heroes(hud, True)
                hero = hud.get_hero_panel()
                hero.refresh_cards()
                cards = hero.get_card_views()
                check('HERO-MISSION-CARDS', len(cards) >= 4)
                check('HERO-CARD-SELECTION', hero.select_mission_hero(prop(cards[0], 'unit_id')))
            elif name == '함대_긴이름':
                for ship in prop(pc,'ships')[:2]:
                    ship.set_editor_property('ship_name','아주 긴 이름을 가진 동일 함급의 함선')
            elif name == '항공대_핀':
                carriers=[prop(c,'carrier') for c in prop(prop(pc,'carrier'),'carriers') if prop(c,'carrier') and prop(prop(prop(c,'carrier'),'systems'),'faction_id')==0]
                check('PIN-AIR-CARRIER-FIXTURE',bool(carriers))
                pc.select_ship(carriers[0]);hud.execute_ui_action('WindowPin');hud.execute_ui_action('PinnedHangar_0')
                check('PIN-AIR-LIVE-SUMMARY',hud.get_pinned_deck(0)==-1 and '생존' in hud.get_pinned_carrier_summary(0))
                other=next(s for s in prop(pc,'ships') if s!=carriers[0] and prop(s,'friendly'))
                pc.select_ship(other);hud.set_details_open(True)
            elif name == '외부_무장':
                hud.execute_ui_action('External')
                state['before_preview'] = snapshot(pc)
            elif name == '핀_비교':
                ships=prop(pc,'ships')
                if os.environ.get('OV_UI_TOOLTIP_TEST') == '1':
                    ships[0].set_editor_property('ship_name','긴 이름을 가진 함선의 전체 명칭 시험')
                for ship in (ships[0],ships[2]):
                    pc.select_ship(ship);hud.execute_ui_action('WindowPin')
                pc.select_ship(ships[1]);hud.set_details_open(True)
                if os.environ.get('OV_UI_TOOLTIP_TEST') == 'detail':
                    ships[1].set_editor_property('ship_name','매우 긴 함선 이름을 상세창에서 확인하는 시험')
                check('PIN-COMPARE-THREE-SHIPS',hud.get_pinned_count()==2 and hud.get_pinned_ship(0)==ships[0] and hud.get_pinned_ship(1)==ships[2] and prop(pc,'selected')==ships[1])
                first=hud.get_pinned_position(0);second=hud.get_pinned_position(1)
                w,h=pc.get_viewport_size();scale=max(.25,min(1,w/1280,h/740))
                x,y=first.x+14,first.y+15
                hud.handle_pointer(x*scale,y*scale,True,True,0,False,0,0)
                hud.handle_pointer((x-55)*scale,y*scale,False,True,0,False,0,0)
                hud.handle_pointer((x-55)*scale,y*scale,False,False,0,False,0,0)
                check('PIN-COMPARE-DRAG-INDEPENDENT',hud.get_pinned_position(0).x<first.x-50 and hud.get_pinned_position(1)==second and prop(pc,'selected')==ships[1])
                moved=hud.get_pinned_position(0);size=hud.get_pinned_size(0);other_size=hud.get_pinned_size(1)
                x,y=moved.x+size.x-5,moved.y+size.y-5
                hud.handle_pointer(x*scale,y*scale,True,True,0,False,0,0)
                hud.handle_pointer((x+50)*scale,(y+5)*scale,False,True,0,False,0,0)
                hud.handle_pointer((x+50)*scale,(y+5)*scale,False,False,0,False,0,0)
                check('PIN-COMPARE-RESIZE-INDEPENDENT',hud.get_pinned_size(0).x>size.x+40 and hud.get_pinned_size(1)==other_size)
            elif name == '핀_시설':
                hud.set_details_open(True);hud.execute_ui_action('PinnedDeck_0_1')
                check('PIN-DECK-INDEPENDENT',hud.get_pinned_deck(0)==1 and hud.get_pinned_deck(1)==0)
            elif name == '핀_무장':
                hud.set_details_open(True);hud.execute_ui_action('PinnedWeaponNext_0')
                check('PIN-WEAPON-INDEPENDENT',hud.get_pinned_weapon(0)==0 and hud.get_pinned_weapon(1)==-1)
            elif name == '초계함_모델':
                # Explicit art inspection only: original surface anchors remain unfinished.
                unreal.SystemLibrary.execute_console_command(world, 'ov.Presentation.ImportedHull 1', pc)
                pc.start_calibration_range(1)
                pc.select_ship(prop(pc, 'ships')[1])
                hud.execute_ui_action('External')
                mesh = prop(prop(prop(pc, 'selected'), 'hull'), 'static_mesh')
                check('IMPORTED-HULL-IN-SCENE', mesh and mesh.get_path_name() == '/Game/Presentation/Ships/SM_PC14.SM_PC14')
            elif name == '격납고':
                ships = prop(pc, 'ships')
                carrier = prop(pc, 'carrier')
                for ship in ships:
                    if any(prop(h, 'carrier') == ship for h in prop(carrier, 'hangars')) and prop(ship, 'friendly'):
                        pc.select_ship(ship)
                        break
                hud.set_details_open(True)
                hud.execute_ui_action('HangarView')
            elif name == '선내_영웅':
                mission = prop(pc, 'mission')
                pc.select_ship(prop(mission, 'origin'))
                hud.execute_ui_action('InteriorToggle')
                check('INTERIOR-WINDOW-OPEN', prop(hud, 'interior_open'))
                units = prop(prop(pc, 'interior'), 'units')
                heroes = [u for u in units if prop(u, 'ship') == prop(pc, 'selected') and prop(u, 'role') != unreal.OVInteriorRole.TROOPER]
                check('INTERIOR-ACTUAL-HEROES', bool(heroes))
                unit = heroes[0]
                slots = prop(prop(prop(pc, 'selected'), 'systems'), 'layout').get_editor_property('slots')
                deck = next(prop(slot, 'deck_id') for slot in slots if prop(slot, 'slot_id') == prop(unit, 'node_id'))
                hud.execute_ui_action('InteriorDeck_' + str(deck))
                hud.execute_ui_action('InteriorUnit_' + str(prop(unit, 'unit_id')))
                check('INTERIOR-HERO-DECK-SELECTED', prop(hud, 'interior_deck') == deck and prop(unit, 'unit_id') in prop(hud, 'interior_selected_units'))
            elif name in ('전투_알림','쉴드_고갈_알림','적_쉴드_알림'):
                pc.action('MissionAttack')
                if prop(pc,'simulation_paused'):pc.action('Pause')
                pc.action('Speed1')
                feed=hud.get_battle_notice_feed()
                if name == '쉴드_고갈_알림':
                    for ship in prop(pc,'ships'):
                        if prop(ship,'friendly'):
                            systems=prop(ship,'systems');initial=prop(systems,'shield')
                            if initial>1:systems.apply_world_hit(ship.get_actor_location(),unreal.Vector(1,0,0),initial-1,unreal.OVDamageCause.EXTERNAL,True)
                if name == '적_쉴드_알림':
                    for ship in prop(pc,'ships'):
                        if not prop(ship,'friendly'):
                            systems=prop(ship,'systems');initial=prop(systems,'shield')
                            if initial>1:systems.apply_world_hit(ship.get_actor_location(),unreal.Vector(1,0,0),initial-1,unreal.OVDamageCause.EXTERNAL,True)
                for _ in range(1800):
                    pc.step_simulation(.1)
                    notices=prop(feed,'notices')
                    if name=='전투_알림' and notices:break
                    if name=='쉴드_고갈_알림' and any(prop(n,'kind')==unreal.OVBattleNoticeKind.SHIELD_DEPLETED for n in notices):break
                    if name=='적_쉴드_알림' and any(prop(n,'kind')==unreal.OVBattleNoticeKind.SHIELD_DEPLETED and prop(n,'ship_name').startswith('적 ') for n in notices):break
                if name=='전투_알림':check('NOTICE-ACTUAL-COMBAT-HIT',bool(prop(feed,'notices')))
                elif name=='쉴드_고갈_알림':check('NOTICE-ACTUAL-SHIELD-DEPLETION',any(prop(n,'kind')==unreal.OVBattleNoticeKind.SHIELD_DEPLETED for n in prop(feed,'notices')))
                else:
                    matches=[n for n in prop(feed,'notices') if prop(n,'kind')==unreal.OVBattleNoticeKind.SHIELD_DEPLETED and prop(n,'ship_name').startswith('적 ')]
                    check('NOTICE-OBSERVED-ENEMY-SHIELD-ACTUAL',bool(matches) and not prop(matches[-1],'critical'))
                    state['enemy_notice_serial']=prop(matches[-1],'serial')
                pc.action('Pause')
                if not hud.is_battle_feed_open():hud.execute_ui_action('BattleFeedToggle')
                state['notice_count']=len(prop(feed,'notices'))
            elif name == '항공대_위기':
                pc.action('MissionAttack')
                if not prop(pc,'simulation_paused'):pc.action('Pause')
                carrier=prop(pc,'carrier')
                ship=next(s for s in prop(pc,'ships') if prop(s,'friendly') and prop(carrier.get_aircraft_overview(s),'surviving')>=10)
                pc.select_ship(ship)
                carrier.launch(ship)
                inventory=[a for a in prop(carrier,'aircraft') if prop(a,'carrier')==ship and prop(a,'faction_id')==0 and prop(a,'state')!=unreal.OVAircraftState.WRECKED]
                check('AIR-NOTICE-FIXTURE-LAUNCH',len(inventory)>=10 and all(prop(a,'state')!=unreal.OVAircraftState.STORED for a in inventory))
                for craft in inventory[:len(inventory)-len(inventory)//10]:
                    check_id=prop(craft,'aircraft_id')
                    if not carrier.apply_aircraft_damage(check_id,100000):raise AssertionError('시험 기체 피해 실패: '+str(check_id))
                feed=hud.get_battle_notice_feed();notices=prop(feed,'notices')
                check('AIR-NOTICE-REAL-LOSS-THRESHOLD',any(prop(n,'kind')==unreal.OVBattleNoticeKind.AIRCRAFT_CRITICAL and prop(n,'baseline')==len(inventory) for n in notices))
                if not hud.is_battle_feed_open():hud.execute_ui_action('BattleFeedToggle')
                state['notice_count']=len(notices)
            elif name == '항공대_필터':
                check('NOTICE-SELECTED-FILTER',hud.get_battle_feed_filter()==5)
            elif name in ('개발_접기','개발_확대'):
                pc.action('Developer');hud.reset_development_window()
                if name == '개발_접기':
                    hud.toggle_development_minimized()
                    check('DEV-MINIMIZED', prop(hud,'development_minimized') and hud.get_development_window_size().y < 80)
                else:
                    hud.toggle_development_maximized()
                    check('DEV-MAXIMIZED', hud.get_development_window_size().x > 1000)
            elif name.startswith('개발_'):
                pc.action('Developer')
                page = {'개발_함선': 1, '개발_영웅': 2, '개발_무장': 3, '개발_시험': 0}[name]
                panel = hud.get_development_panel()
                check('DEV-PAGE-' + str(page), panel.open_page(page))
            state.update(stage='capture', at=time.monotonic())
            return
        if state['stage'] == 'capture':
            if name == '함대_긴이름' and not state.get('tooltip_cursor_set'):
                center=hud.get_action_center('Pick_0');w,h=pc.get_viewport_size();scale=max(.25,min(1,w/1280,h/740))
                check('FLEET-TOOLTIP-TEXT',hud.get_fleet_title_tooltip(center)=='아주 긴 이름을 가진 동일 함급의 함선 1')
                pc.set_mouse_location(int(center.x*scale),int(center.y*scale))
                state.update(tooltip_cursor_set=True,at=time.monotonic());return
            if name == '초계함_모델' and time.monotonic()-state['at'] < 5:
                return  # First-time source texture/shader preparation can outlast a normal frame.
            if name == '외부_무장':
                check('PREVIEW-MESH-MIRROR', hud.get_preview_mesh_count() > 0, hud.get_preview_mesh_count())
                check('WEAPON-PREVIEW-SELECTED-MOUNT', hud.get_weapon_preview_focus() == prop(pc, 'selected_weapon'))
                check('WEAPON-PREVIEW-ONLY-MOUNT-MESHES', 0 < hud.get_weapon_preview_visible_mesh_count() < hud.get_preview_mesh_count(), hud.get_weapon_preview_visible_mesh_count())
                systems=prop(prop(pc,'selected'),'systems');index=prop(pc,'selected_weapon')
                hp=prop(prop(systems,'weapon_mount_states')[index],'hp');maximum=prop(prop(prop(systems,'layout'),'weapon_mounts')[index],'max_hp')
                check('WEAPON-PREVIEW-ACTUAL-HEALTH', maximum > 0 and abs(hud.get_selected_weapon_health_ratio()-hp/maximum)<.001)
                check('PREVIEW-NO-DAMAGE', state['before_preview'] == snapshot(pc))
            if name == '핀_비교':
                for i in range(2):
                    action='Pinned_'+str(i);center=hud.get_action_center(action)
                    check('PIN-COMPARE-HITBOX-'+str(i),center.x>=0 and str(hud.get_hit_action_at_canvas_point(center))==action)
                if os.environ.get('OV_UI_TOOLTIP_TEST') == '1' and not state.get('tooltip_cursor_set'):
                    center=hud.get_action_center('Pinned_0');w,h=pc.get_viewport_size();scale=max(.25,min(1,w/1280,h/740))
                    check('PIN-TOOLTIP-TEXT',hud.get_pinned_title_tooltip(center)=='긴 이름을 가진 함선의 전체 명칭 시험')
                    pc.set_mouse_location(int(center.x*scale),int(center.y*scale))
                    state.update(tooltip_cursor_set=True,at=time.monotonic());return
                if os.environ.get('OV_UI_TOOLTIP_TEST') == 'detail' and not state.get('tooltip_cursor_set'):
                    center=prop(hud,'window_position')+unreal.Vector2D(20,15);w,h=pc.get_viewport_size();scale=max(.25,min(1,w/1280,h/740))
                    check('DETAIL-TOOLTIP-TEXT',hud.get_details_title_tooltip(center)=='매우 긴 함선 이름을 상세창에서 확인하는 시험')
                    pc.set_mouse_location(int(center.x*scale),int(center.y*scale))
                    state.update(tooltip_cursor_set=True,at=time.monotonic());return
            if name == '핀_시설':
                slots=prop(prop(prop(hud.get_pinned_ship(0),'systems'),'layout'),'slots')
                index=next(i for i,s in enumerate(slots) if prop(s,'deck_id')==1)
                action='PinnedSlot_0_'+str(index);center=hud.get_action_center(action)
                check('PIN-FACILITY-HITBOX',center.x>=0 and str(hud.get_hit_action_at_canvas_point(center))==action)
                hud.execute_ui_action(action);check('PIN-FACILITY-SELECTED',hud.get_pinned_slot(0)==index)
            if name == '핀_무장':
                check('PIN-WEAPON-LIVE-FOCUS',hud.get_pinned_preview_focus(0)==hud.get_pinned_weapon(0) and 'HP' in hud.get_pinned_weapon_summary(0))
            if name == '항공대_필터':
                notices=prop(hud.get_battle_notice_feed(),'notices')
                own=next(n for n in reversed(notices) if not prop(n,'fleet_scope') and prop(n,'kind')==unreal.OVBattleNoticeKind.AIRCRAFT_CRITICAL)
                fleet=next(n for n in reversed(notices) if prop(n,'fleet_scope') and prop(n,'kind')==unreal.OVBattleNoticeKind.AIRCRAFT_CRITICAL)
                check('NOTICE-SELECTED-CARRIER-VISIBLE',hud.get_action_center('BattleNotice_'+str(prop(own,'serial'))).x>=0)
                check('NOTICE-FLEET-NOT-MISATTRIBUTED',hud.get_action_center('BattleNotice_'+str(prop(fleet,'serial'))).x<0)
            if name == '적_쉴드_알림':
                action='BattleNotice_'+str(state['enemy_notice_serial']);center=hud.get_action_center(action)
                check('NOTICE-OBSERVED-ENEMY-VISIBLE',center.x>=0 and str(hud.get_hit_action_at_canvas_point(center))==action)
            if name in ('전투_알림','쉴드_고갈_알림','항공대_위기'):
                index=state['notice_count']-1;feed=hud.get_battle_notice_feed();action='BattleNotice_'+str(prop(prop(feed,'notices')[index],'serial'))
                center=hud.get_action_center(action)
                check('NOTICE-VISIBLE-HITBOX',center.x>=0 and str(hud.get_hit_action_at_canvas_point(center))==action)
                target=prop(prop(feed,'notices')[index],'last_position')
                hud.execute_ui_action(action)
                check('NOTICE-FOCUS-LAST-IMPACT',(prop(pc,'camera_focus')-target).length()<.1)
            path = os.path.join(out, '%02d_%s.png' % (state['index'] + 1, name))
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="%s" -nosuffix' % path.replace('\\', '/'), pc)
            state['pending'] = {'view': name, 'path': path, 'viewport_size': list(pc.get_viewport_size())}
            state.update(stage='verify', at=time.monotonic())
            return
        if state['stage'] == 'verify':
            item = state['pending']
            item['exists'] = os.path.isfile(item['path']) and os.path.getsize(item['path']) > 0
            if not item['exists'] and time.monotonic()-state['at'] < 15:
                return
            check('RENDER-' + name, item['exists'])
            report['captures'].append(item)
            if name == '항공대_위기':
                hud.execute_ui_action('BattleFeedOlder')
                check('NOTICE-OLDER-PAGE-OPENED',hud.get_battle_feed_page()==1)
                state.update(stage='notice_older',at=time.monotonic());return
            if name == '핀_시설':
                hud.execute_ui_action('PinnedClose_1');hud.execute_ui_action('PinnedClose_0')
            if name == '외부_무장':
                hud.execute_ui_action('Weapon_1')
                state.update(stage='weapon_switched', at=time.monotonic())
                return
            state['index'] += 1
            if state['index'] >= len(views):
                finish()
            else:
                state.update(stage='prepare', at=time.monotonic())
        elif state['stage'] == 'weapon_switched':
            check('WEAPON-PREVIEW-SWITCH-MOUNT', prop(pc,'selected_weapon') == 1 and hud.get_weapon_preview_focus() == 1 and hud.get_weapon_preview_visible_mesh_count() > 0)
            check('WEAPON-PREVIEW-SWITCH-NO-DAMAGE', state['before_preview'] == snapshot(pc))
            hud.set_details_open(False)
            state.update(stage='weapon_closed', at=time.monotonic())
        elif state['stage'] == 'weapon_closed':
            check('WEAPON-PREVIEW-CLOSE-RELEASES', hud.get_weapon_preview_focus() == -1 and hud.get_weapon_preview_visible_mesh_count() == 0)
            state['index'] += 1
            state.update(stage='prepare', at=time.monotonic())
        elif state['stage'] == 'notice_older':
            feed=hud.get_battle_notice_feed();notices=prop(feed,'notices');notice=notices[len(notices)-4]
            action='BattleNotice_'+str(prop(notice,'serial'));center=hud.get_action_center(action)
            check('NOTICE-OLDER-STABLE-HITBOX',center.x>=0 and str(hud.get_hit_action_at_canvas_point(center))==action)
            hud.execute_ui_action(action)
            check('NOTICE-OLDER-LAST-POSITION',(prop(pc,'camera_focus')-prop(notice,'last_position')).length()<.1)
            hud.execute_ui_action('BattleFeedNewer');check('NOTICE-NEWER-PAGE-OPENED',hud.get_battle_feed_page()==0)
            hud.execute_ui_action('BattleFeedFilter');hud.execute_ui_action('BattleFeedFilter')
            check('NOTICE-DAMAGE-FILTER',hud.get_battle_feed_filter()==2)
            state.update(stage='notice_empty',at=time.monotonic())
        elif state['stage'] == 'notice_empty':
            center=hud.get_action_center('BattleFeedFilter')
            check('NOTICE-EMPTY-FILTER-REMAINS-ACCESSIBLE',center.x>=0 and str(hud.get_hit_action_at_canvas_point(center))=='BattleFeedFilter')
            center=hud.get_action_center('BattleWarningLevel')
            check('WARNING-LEVEL-CLICKABLE',center.x>=0 and str(hud.get_hit_action_at_canvas_point(center))=='BattleWarningLevel')
            w,h=pc.get_viewport_size();scale=max(.25,min(1,w/1280,h/740))
            check('WARNING-LEVEL-CONSUMES-WORLD-INPUT',hud.is_over_panel(center.x*scale,center.y*scale))
            events=len(prop(hud.get_battle_notice_feed(),'notices'));before=snapshot(pc)
            check('WARNING-DEFAULT',hud.get_warning_level()==2)
            hud.execute_ui_action('BattleWarningLevel');check('WARNING-LOW',hud.get_warning_level()==1)
            hud.execute_ui_action('BattleWarningLevel');check('WARNING-OFF',hud.get_warning_level()==0)
            hud.execute_ui_action('BattleWarningLevel');check('WARNING-RESTORE',hud.get_warning_level()==2)
            check('WARNING-SETTING-NO-COMBAT-CHANGE',events==len(prop(hud.get_battle_notice_feed(),'notices')) and before==snapshot(pc))
            hud.execute_ui_action('BattleFeedFilter');hud.execute_ui_action('BattleFeedFilter');hud.execute_ui_action('BattleFeedFilter')
            state['index'] += 1;state.update(stage='prepare',at=time.monotonic())
    except Exception:
        report['errors'].append(traceback.format_exc())
        unreal.log_error(report['errors'][-1])
        finish()

state['handle'] = unreal.register_slate_post_tick_callback(tick)
