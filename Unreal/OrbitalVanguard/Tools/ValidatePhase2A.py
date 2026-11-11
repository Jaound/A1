"""Real Editor PIE integration via APIs. Does NOT inject OS mouse/keyboard input.
Start editor with -ExecCmds="py <absolute path>". Leaves PIE paused for inspection.
The historical phase 1 ValidatePIE.py is preserved; this suite covers changed 2A semantics.
"""
import unreal, time, json, os, traceback

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(root, 'Validation'), exist_ok=True)
report = {'kind': 'Editor PIE API; not physical input', 'planning': 'cb96d2c', 'checks': [], 'errors': []}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {'phase': 0, 'start': time.monotonic()}
def p(obj, name): return obj.get_editor_property(name)
def check(name, value, detail=''):
    report['checks'].append({'name': name, 'passed': bool(value), 'detail': str(detail)})
    unreal.log('OV2 CHECK: ' + name + ' = ' + str(bool(value)) + ' ' + str(detail))
    if not value: raise AssertionError(name)
def near(a, b, tolerance=.1):
    fields = ('x', 'y', 'z') if isinstance(a, unreal.Vector) else ('pitch', 'yaw', 'roll')
    return all(abs(getattr(a, k)-getattr(b, k)) <= tolerance for k in fields)
def save():
    report['passed'] = bool(report['checks']) and all(c['passed'] for c in report['checks']) and not report['errors']
    with open(os.path.join(root, 'Validation', 'Phase2A.json'), 'w', encoding='utf-8') as out:
        json.dump(report, out, indent=2)
def stop(): unreal.unregister_slate_post_tick_callback(state['handle'])
def frames(pc, count, dt=1/60):
    for _ in range(count): pc.step_simulation(dt)
def pause(pc, value):
    if p(pc, 'simulation_paused') != value: pc.action('Pause')
def reset(ship, location):
    p(ship, 'systems').reset_state(); ship.debug_reset_motion()
    ship.set_actor_location(location, False, False)
    ship.set_actor_rotation(unreal.Rotator(), False)

def run(pc, ships, world):
    a, b, target = ships
    sa, sb, st = [p(s, 'systems') for s in ships]
    hud = pc.get_hud()
    check('DataAsset layouts loaded, 14/26/38 independent armor',
          [len(p(s, 'armor_sections')) for s in (sa, sb, st)] == [14, 26, 38]
          and all(p(s, 'layout').get_path_name().startswith('/Game/Layouts/') for s in (sa, sb, st)))
    check('Layout validation from serialized assets', all(p(s, 'layout').validate_layout() is not None and len(p(s, 'layout').validate_layout()) == 0 for s in (sa, sb, st)))
    pc.action('Ship2'); check('Selection while paused', p(pc, 'selected') == b)
    cam = p(pc, 'range_camera'); old = cam.get_actor_location()
    pc.set_editor_property('camera_focus', unreal.Vector(300, 400, 500)); pc.update_camera()
    check('Paused camera pan', not near(old, cam.get_actor_location()))
    old = cam.get_actor_rotation(); pc.set_editor_property('camera_yaw', 15); pc.set_editor_property('camera_pitch', -35); pc.update_camera()
    check('Paused camera orbit', not near(old, cam.get_actor_rotation()))
    old = cam.get_actor_location(); pc.set_editor_property('camera_distance', 4700); pc.update_camera()
    check('Paused camera zoom', not near(old, cam.get_actor_location()))
    pc.action('Move')
    z = b.get_actor_location().z
    pc.sample_order_cursor(unreal.Vector(100, 200, 3000), unreal.Vector(0, 0, -1), 300, False, False)
    pc.sample_order_cursor(unreal.Vector(100, 200, 3000), unreal.Vector(0, 0, -1), 300, True, False)
    pc.sample_order_cursor(unreal.Vector(999, 999, 3000), unreal.Vector(0, 0, -1), 280, True, False)
    check('Anchored altitude drag locks XY', near(p(pc, 'preview'), unreal.Vector(100, 200, z+200)))
    before = p(pc, 'preview'); pc.sample_order_cursor(unreal.Vector(999, 999, 3000), unreal.Vector(0, 0, -1), 50, True, True)
    check('UI blocked drag preserves preview', near(before, p(pc, 'preview')))
    pc.action('Cancel'); pc.action('Move'); pc.set_editor_property('preview', unreal.Vector(-200, 1600, 1400)); pc.action('Confirm')
    old = b.get_actor_location(); pc.step_simulation(5)
    check('Paused XYZ order persists without movement', p(b, 'has_destination') and near(old, b.get_actor_location()))
    pause(pc, False); pc.action('Speed1'); frames(pc, 1800)
    check('XYZ accelerated arrival stops', not p(b, 'has_destination') and near(b.get_actor_location(), unreal.Vector(-200, 1600, 1400)), b.get_actor_location())
    b.set_manual_attitude(unreal.Rotator(pitch=15, yaw=90, roll=20)); frames(pc, 300)
    check('Manual pitch roll turn target', near(b.get_actor_rotation(), unreal.Rotator(pitch=15, yaw=90, roll=20), .2))
    pc.action('Auto'); b.stop_movement(); old = b.get_actor_location(); rot = b.get_actor_rotation(); frames(pc, 120)
    check('Auto preserves Hold and manual posture', not p(b, 'manual') and p(b, 'hold') and near(old, b.get_actor_location()) and near(rot, b.get_actor_rotation()))
    pc.action('Manual'); pc.set_target(target)
    check('Target does not switch manual mode or move', p(b, 'manual') and p(b, 'attack_target') == target and p(b, 'hold'))
    pc.action('ClearTarget'); check('Clear target independent', p(b, 'attack_target') is None)
    pc.action('Focus'); check('Follow toggles on', p(pc, 'follow')); pc.action('Focus'); check('Follow toggles off', not p(pc, 'follow'))
    # Same ten seconds of game time at each speed, including acceleration and shields.
    samples = []
    for speed in (1, 2, 3):
        reset(a, unreal.Vector(-4000, -4000, 300)); pc.select_ship(a)
        a.set_destination(unreal.Vector(8000, -4000, 300)); pc.action('Speed'+str(speed))
        sa.apply_world_hit(unreal.Vector(-4000, -4000, 342), unreal.Vector(0, 0, 1), p(sa, 'shield'), unreal.OVDamageCause.EXTERNAL)
        start = p(pc, 'simulation_seconds'); frames(pc, 600//speed)
        samples.append((a.get_actor_location(), p(sa, 'shield')))
        check('Common simulation clock '+str(speed)+'x', abs(p(pc, 'simulation_seconds')-start-10) < .005)
    check('Same game-time motion and shield at 1/2/3x', all(near(samples[0][0], x[0], .5) and abs(samples[0][1]-x[1]) < .1 for x in samples[1:]), samples)
    pc.action('Speed1')
    reset(a, unreal.Vector(0, -3000, 300)); reset(b, unreal.Vector(0, -3000, 300))
    before = p(pc, 'pair_resolution_count'); pc.step_simulation(1/60)
    check('Overlapping pair hard separates once', p(pc, 'pair_resolution_count')-before == 1 and (a.get_actor_location()-b.get_actor_location()).length() >= 389)
    # Simulated FTL state only: travel implementation belongs to later phases.
    a.debug_set_ftl_charging(True); fixed = a.get_actor_location()
    b.set_actor_location(fixed, False, False); pc.step_simulation(1/60)
    check('FTL charging cannot be pushed', near(a.get_actor_location(), fixed))
    a.set_destination(unreal.Vector(999, 999, 999)); check('FTL rejects destination', not p(a, 'has_destination'))
    a.debug_set_ftl_charging(False)
    # Rotated and nonuniformly scaled world-to-local hit must retain the exact face/room.
    reset(a, unreal.Vector(-1000, -650, 300)); a.set_actor_rotation(unreal.Rotator(pitch=20, yaw=37, roll=25), False)
    a.set_actor_scale3d(unreal.Vector(1.4, 1.2, 1.8))
    layout = p(sa, 'layout'); local = unreal.Vector(175, 0, 0)
    worldhit = unreal.MathLibrary.transform_location(a.get_actor_transform(), local)
    normal = unreal.MathLibrary.transform_direction(a.get_actor_transform(), unreal.Vector(1, 0, 0))
    expected = layout.resolve_hit(local, unreal.Vector(1, 0, 0))
    sa.apply_world_hit(worldhit, normal, p(sa, 'shield')+40, unreal.OVDamageCause.EXTERNAL)
    check('Rotated rolled scaled world hit mapping', p(sa, 'last_damage_index') == expected and near(p(sa, 'last_local_hit'), local, .01))
    pc.select_ship(target); pc.action('Armor_37'); pc.select_additional(a); pc.select_additional(target)
    check('Representative switch resets detail indices', p(pc, 'selected') == a and p(pc, 'selected_armor') == 0)
    pc.action('ArmorZero'); check('DEV selected armor ID matches state', p(p(sa, 'armor_sections')[0], 'hp') == 0)
    pc.action('RepairArmor'); check('DEV repair armor restores only section', p(p(sa, 'armor_sections')[0], 'hp') == p(p(layout, 'armor_sections')[0], 'max_armor'))
    hull = p(sa, 'hull_hp'); pc.action('WeaponHit'); check('Independent weapon HP damage leaves hull', p(sa, 'hull_hp') == hull and p(p(sa, 'weapon_mount_states')[0], 'hp') < 200)
    pc.action('RepairWeapon')
    pc.select_ship(target); pc.action('Deck3'); check('Third floor selection', p(pc, 'viewed_deck') == 3)
    pc.action('Developer'); pc.action('Deck3'); check('Enemy floors blocked outside DEV', p(pc, 'viewed_deck') == 0)
    pc.action('Developer'); pc.clear_selection(); check('Deselect clears selection and orders', not p(pc, 'selected') and not p(pc, 'placing'))
    pc.select_ship(a); pc.action('Move')
    # HUD API receives pixels, not OS mouse events. Fixed high corner is within its top bar.
    old = p(pc, 'preview'); check('HUD top bar consumes pointer API', hud.handle_pointer(20, 20, True, True, 1, True, 15, 15))
    check('HUD API does not mutate world preview', near(old, p(pc, 'preview')))
    hud.handle_pointer(20, 20, False, False, 0, False, 0, 0); pc.action('Cancel')
    width, height = pc.get_viewport_size()
    ui_scale = max(.25, min(1, width/1280, height/740))
    window = p(hud, 'window_position'); initial = (window.x, window.y)
    def pointer(x, y, pressed=False, down=False, wheel=0, orbit=False, dx=0, dy=0):
        return hud.handle_pointer(x*ui_scale, y*ui_scale, pressed, down, wheel, orbit, dx, dy)
    pointer(window.x+12, window.y+15, True, True)
    pointer(window.x-88, window.y+25, False, True)
    pointer(window.x-88, window.y+25)
    window = p(hud, 'window_position')
    check('Floating title drag changes window position', abs(window.x-initial[0]) > 1)
    size = p(hud, 'window_size'); old_width = size.x
    pointer(window.x+size.x-5, window.y+size.y-5, True, True)
    pointer(window.x+size.x+30, window.y+size.y+20, False, True)
    pointer(window.x+size.x+30, window.y+size.y+20)
    check('Floating resize API works outside draw callback', p(hud, 'window_size').x > old_width)
    size = p(hud, 'window_size'); before_size = (size.x, size.y)
    pointer(width/ui_scale-2, height/ui_scale-2, True, True)
    pointer(width/ui_scale-1, height/ui_scale-1, False, True)
    pointer(width/ui_scale-1, height/ui_scale-1)
    size = p(hud, 'window_size')
    check('Bottom toolbar outside handle cannot start resizing', (size.x, size.y) == before_size)
    rot = a.get_actor_rotation(); camera_location = cam.get_actor_location()
    pointer(window.x+100, window.y+150, wheel=1, orbit=True, dx=20, dy=15)
    check('Read-only preview orbit does not move ship or world camera', near(rot, a.get_actor_rotation()) and near(camera_location, cam.get_actor_location()))
    count = len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.OVShip))
    for _ in range(5): pc.action('Details'); pc.action('External'); pc.action('Deck1')
    check('Read-only viewer adds no simulation actors', len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.OVShip)) == count == 3)
    pc.set_target(target); a.set_destination(unreal.Vector(5000, 5000, 500)); frames(pc, 10)
    sa.debug_destroy_slot(p(layout, 'core_slot_id'), unreal.OVDamageCause.SABOTAGE); pc.step_simulation(1/60)
    check('Wreck immediately clears movement velocity and target', p(sa, 'wrecked') and p(a, 'hold') and not p(a, 'has_destination') and p(a, 'attack_target') is None and p(a, 'velocity').length() == 0)
    hp = p(sa, 'hull_hp'); sa.debug_repair_slot(p(layout, 'core_slot_id')); pc.action('Move')
    check('Wreck cannot be repaired or commanded', p(sa, 'wrecked') and p(sa, 'hull_hp') == hp and not p(pc, 'placing'))
    for s, location in zip(ships, (unreal.Vector(-1000,-650,300), unreal.Vector(-800,650,300), unreal.Vector(1900,900,1100))): reset(s, location)
    a.set_actor_scale3d(unreal.Vector(1.4,1.4,1.4))
    pc.action('Speed1'); pause(pc, True); pc.select_ship(b); pc.action('External')
    pc.set_editor_property('camera_focus', unreal.Vector()); pc.set_editor_property('camera_yaw', -45); pc.set_editor_property('camera_pitch', -48); pc.set_editor_property('camera_distance', 6500); pc.update_camera()

def tick(dt):
    try:
        if time.monotonic()-state['start'] > 120: raise TimeoutError('PIE validation timeout')
        if state['phase'] == 0:
            check('Editor map loaded', editor.get_editor_world().get_name() == 'ControlRange')
            level.editor_request_begin_play(); state['phase'] = 1; return
        if state['phase'] == 1:
            world = editor.get_game_world()
            if not world: return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if not pc or len(p(pc, 'ships')) != 3: return
            check('Real PIE world', level.is_in_play_in_editor() and 'UEDPIE' in world.get_path_name(), world.get_path_name())
            pause(pc, True); state.update(pc=pc, ships=p(pc,'ships'), world=world, at=time.monotonic(), sim=p(pc,'simulation_seconds'), phase=2)
            return
        if state['phase'] == 2:
            if time.monotonic()-state['at'] < 1: return
            check('Pause freezes real frame simulation', p(state['pc'],'simulation_seconds') == state['sim'])
            run(state['pc'], state['ships'], state['world'])
            a = state['ships'][0]; state['real_pos'] = a.get_actor_location()
            a.set_destination(unreal.Vector(1500,-650,900)); pause(state['pc'], False)
            state['real_sim'] = p(state['pc'], 'simulation_seconds')
            state['phase']=3; state['at']=time.monotonic(); return
        if state['phase'] == 3:
            if time.monotonic()-state['at'] < 2: return
            check('Real PIE frames advance motion and clock', not near(state['ships'][0].get_actor_location(), state['real_pos'], 1) and p(state['pc'], 'simulation_seconds') > state['real_sim']+.1)
            pause(state['pc'], True); state['phase']=4; state['at']=time.monotonic(); return
        if state['phase'] == 4:
            if time.monotonic()-state['at'] < 2: return
            state['pc'].select_ship(state['ships'][1]); state['pc'].action('External')
            unreal.SystemLibrary.execute_console_command(state['world'], 'Shot SHOWUI', state['pc'])
            save(); stop(); unreal.log('OV2 VALIDATE COMPLETE')
    except Exception:
        report['errors'].append(traceback.format_exc()); save(); unreal.log_error(report['errors'][-1]); stop()
state['handle'] = unreal.register_slate_post_tick_callback(tick)
