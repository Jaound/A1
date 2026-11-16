"""2B integration in a real Editor PIE world through reflected APIs.

This is NOT physical mouse/keyboard input. It preserves the old 2A validator.
The initial beam checkpoint records its limited scope; all-kind validation enables
automatically when the complete weapon profile table is available.
Run: UnrealEditor.exe <project> /Game/Maps/ControlRange -ExecCmds="py <this file>"
Output: Saved/Validation/Phase2B.json. Leaves a fresh, paused 2B range for the user.
"""
import json
import os
import time
import traceback
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
os.makedirs(os.path.join(root, 'Validation'), exist_ok=True)
report = {'kind': 'Editor PIE API; not physical mouse/keyboard input',
          'planning': 'cac0a6f; approved D083-D102 policies', 'seed': 260112, 'checks': [], 'errors': [],
          'physical_input': 'not performed by this script'}
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
state = {'phase': 0, 'start': time.monotonic()}


def p(obj, name):
    return obj.get_editor_property(name)


def check(name, passed, detail=''):
    report['checks'].append({'name': name, 'passed': bool(passed), 'detail': str(detail)})
    unreal.log('OV2B CHECK: ' + name + ' = ' + str(bool(passed)) + ' ' + str(detail))
    if not passed:
        raise AssertionError(name + ': ' + str(detail))


def save():
    report['passed'] = bool(report['checks']) and all(c['passed'] for c in report['checks']) and not report['errors']
    with open(os.path.join(root, 'Validation', 'Phase2B.json'), 'w', encoding='utf-8') as out:
        json.dump(report, out, ensure_ascii=False, indent=2)


def pause(pc, desired):
    if p(pc, 'simulation_paused') != desired:
        pc.action('Pause')


def frames(pc, number, dt=1/60):
    for _ in range(number):
        pc.step_simulation(dt)


def move_window(pc, x, y):
    hud = pc.get_hud()
    width, height = pc.get_viewport_size()
    scale = max(.25, min(1, width/1280, height/740))
    position = p(hud, 'window_position')
    hud.handle_pointer((position.x+12)*scale, (position.y+15)*scale, True, True, 0, False, 0, 0)
    hud.handle_pointer((x+12)*scale, (y+15)*scale, False, True, 0, False, 0, 0)
    hud.handle_pointer((x+12)*scale, (y+15)*scale, False, False, 0, False, 0, 0)


def mount(combat, ship, index=1):
    return p(combat.get_ship_state(ship), 'mounts')[index]


def weapon_id(combat, ship, index=1):
    return p(mount(combat, ship, index), 'mount_id')


def facility_disable(ship, kind):
    systems = p(ship, 'systems')
    for slot in p(p(systems, 'layout'), 'slots'):
        if p(slot, 'facility') == kind:
            systems.debug_set_crew(p(slot, 'slot_id'), 0)


def fixture(pc, shooter_index=0):
    pc.start_combat_range()
    ships = p(pc, 'ships')
    a, support, target = ships[shooter_index], ships[1-shooter_index], ships[2]
    for ship in (a, support, target):
        ship.debug_reset_motion()
        ship.set_actor_rotation(unreal.Rotator(), False)
    a.set_actor_location(unreal.Vector(0, 0, 300), False, False)
    support.set_actor_location(unreal.Vector(0, 15000, 300), False, False)
    target.set_actor_location(unreal.Vector(1200, 0, 300), False, False)
    pc.select_ship(a)
    pc.set_target(target)
    pc.action('Weapon_1')
    pause(pc, False)
    frames(pc, 240)
    return p(pc, 'combat'), a, support, target


def custom_profile(combat, ship, index=1, **values):
    settings = p(combat, 'settings')
    profile = p(mount(combat, ship, index), 'profile')
    # Reflected struct values are copied before assignment; this edits only the
    # subsystem's per-run settings, never the Config CDO or saved layout assets.
    profile.set_editor_property('profile_id', 'Validation_' + str(index))
    for name, value in values.items():
        profile.set_editor_property(name, value)
    profiles = list(p(settings, 'weapons'))
    profiles = [x for x in profiles if str(p(x, 'profile_id')) != 'Validation_' + str(index)]
    profiles.append(profile)
    settings.set_editor_property('weapons', profiles)
    if not combat.set_weapon_profile(ship, weapon_id(combat, ship, index), p(profile, 'profile_id')):
        raise AssertionError('Could not apply transient validation profile')


def run_common(pc, world):
    pc.set_editor_property('developer_mode', True)
    pc.get_hud().set_details_open(True)
    camera_profile = p(pc, 'play_game3')
    combat, a, support, target = fixture(pc)
    sa, st = p(a, 'systems'), p(target, 'systems')
    profiles = p(p(combat, 'settings'), 'weapons')
    kinds = {p(profile, 'kind') for profile in profiles}
    full = len(kinds) == 4
    report['scope'] = 'full_2B' if full else 'beam_checkpoint'
    check('Opt-in range enabled and 2A camera profile preserved', p(combat, 'enabled') and p(pc, 'play_game3') == camera_profile)
    check('Combat fixture is transient; authored 2A assets remain separate', all(not p(p(s, 'systems'), 'layout').get_path_name().startswith('/Game/Layouts/') for s in (a, support, target)))
    check('Formal detection is default', not p(p(combat, 'settings'), 'development_omniscient'))
    check('Manual target selection does not fire or switch mode', p(combat, 'shot_count') == 0 and p(a, 'manual') and p(a, 'attack_target') == target)
    ready = combat.get_readiness(a, weapon_id(combat, a))
    check('Aligned live beam READY with detection source', p(ready, 'ready') and p(p(ready, 'detection'), 'detected'), p(ready, 'reason'))
    before = p(st, 'shield')
    pc.action('Fire')
    check('Fire action requests; UI does not directly apply damage', p(combat, 'shot_count') == 0 and p(st, 'shield') == before)
    pc.step_simulation(1/60)
    check('Requested beam launches once and hits existing shield path', p(combat, 'shot_count') == 1 and p(combat, 'hit_count') == 1 and p(st, 'shield') < before)
    check('Shield-only hit records event without independent hull damage', p(st, 'hit_event_id') == 1 and p(st, 'last_hull_damage') == 0)
    ready = combat.get_readiness(a, weapon_id(combat, a))
    check('Cooldown displayed by same launch evaluator', not p(ready, 'ready') and p(ready, 'cooldown') > 0, p(ready, 'reason'))
    frames(pc, 180)
    check('One manual request does not repeat after cooldown', p(combat, 'shot_count') == 1)
    a.set_manual_attitude(unreal.Rotator(0, 0, 20))
    frames(pc, 180)
    rotation = a.get_actor_rotation()
    location = a.get_actor_location()
    pc.action('Auto')
    frames(pc, 60)
    check('Auto fires while preserving Hold and manual hull posture', p(combat, 'shot_count') > 1 and p(a, 'hold') and (a.get_actor_location()-location).length() < .01 and abs(a.get_actor_rotation().roll-rotation.roll) < .01)
    pc.action('Manual')
    pause(pc, True)
    shots = p(combat, 'shot_count')
    game_time = p(combat, 'game_time')
    pc.action('Fire')
    pc.step_simulation(20)
    check('Pause freezes request consumption and projectile simulation', p(combat, 'shot_count') == shots and p(combat, 'game_time') == game_time)

    combat, a, support, target = fixture(pc)
    custom_profile(combat, a, range=100)
    pc.action('Fire'); frames(pc, 30)
    check('Range blocks actual launch and reports reason', p(combat, 'shot_count') == 0 and 'RANGE' in p(combat.get_readiness(a, weapon_id(combat, a)), 'reason'))
    combat, a, support, target = fixture(pc)
    facility_disable(a, unreal.OVFacilityType.RADAR)
    target.set_actor_location(unreal.Vector(3000, 0, 300), False, False)
    pc.action('Fire'); frames(pc, 30)
    check('No own or shared detection blocks new launch', p(combat, 'shot_count') == 0 and not p(combat.get_detection(a, target), 'detected'))
    support.set_actor_location(unreal.Vector(2500, 2000, 300), False, False)
    detection = combat.get_detection(a, target)
    check('Radar support provides strongest valid shared source', p(detection, 'detected') and 'SUPPORT' in p(detection, 'source'), p(detection, 'source'))
    combat, a, support, target = fixture(pc)
    target.set_actor_location(unreal.Vector(1200, 0, -1000), False, False)
    pc.action('Fire'); frames(pc, 120)
    check('Local underside blind arc blocks top mount', p(combat, 'shot_count') == 0 and 'ARC' in p(combat.get_readiness(a, weapon_id(combat, a)), 'reason'))
    for kind in (unreal.OVFacilityType.WEAPON_CONTROL, unreal.OVFacilityType.CAPACITOR):
        combat, a, support, target = fixture(pc)
        facility_disable(a, kind)
        pc.action('Fire'); frames(pc, 60)
        check('Stopped facility blocks live launch ' + str(kind), p(combat, 'shot_count') == 0 and not p(combat.get_readiness(a, weapon_id(combat, a)), 'ready'))
    combat, a, support, target = fixture(pc)
    a.debug_set_ftl_charging(True)
    pc.action('Fire'); frames(pc, 30)
    check('FTL charging blocks beam, distinct from departure', p(combat, 'shot_count') == 0 and not combat.is_departed(a))
    a.debug_set_ftl_charging(False)
    p(a, 'systems').debug_hit_weapon(weapon_id(combat, a), 9999)
    pc.action('Fire'); frames(pc, 30)
    check('Individual mount HP blocks actual firing', p(combat, 'shot_count') == 0 and 'MOUNT' in p(combat.get_readiness(a, weapon_id(combat, a)), 'reason'))
    combat, a, support, target = fixture(pc)
    pc.action('AutoFree')
    check('Explicit AUTO FREE releases Hold without altering camera or movement stats', not p(a, 'hold') and not p(a, 'manual') and p(pc, 'play_game3') == camera_profile)
    a.stop_movement(); a.set_manual_attitude(unreal.Rotator(0, 0, 20))
    pc.action('AutoFree')
    check('AUTO FREE respects a deliberate manual hull posture', p(a, 'hold') and p(a, 'has_manual_attitude'))
    combat, a, support, target = fixture(pc)
    target.set_actor_location(a.get_actor_location()+unreal.Vector(1,0,0), False, False)
    pc.select_ship(target)
    before_pos = a.get_actor_location()
    combat.notify_ftl_departure(target); frames(pc, 3)
    check('FTL departure removes selection and contact influence', p(pc, 'selected') is None and (a.get_actor_location()-before_pos).length() < .001 and combat.is_departed(target))
    # Fixed seeds and fixed combat time through the real controller speed controls.
    samples = []
    for speed in (1, 2, 3):
        combat, a, support, target = fixture(pc, 1 if full else 0)
        pc.action('Speed'+str(speed)); pc.action('Auto')
        frames(pc, 360//speed)
        samples.append((p(combat, 'shot_count'), p(combat, 'hit_count'), len(p(combat, 'attacks')), round(p(p(target, 'systems'), 'hull_hp'), 3), round(p(p(target, 'systems'), 'shield'), 3)))
    check('Same seed and game time give identical shots damage and active projectiles at 1/2/3x', samples[0] == samples[1] == samples[2], samples)
    if full:
        run_projectiles(pc)
        run_approved_policies(pc)
    # Fresh actual demo, both sides automatic, then observe real slate/PIE frames.
    pc.start_combat_range()
    a, support, target = p(pc, 'ships')
    pc.select_ship(a)
    pc.set_target(target)
    pc.action('Auto')
    pause(pc, False)
    state.update(pc=pc, a=a, target=target, combat=p(pc, 'combat'), at=time.monotonic())


def run_projectiles(pc):
    for kind, index in ((unreal.OVAttackKind.CANNON, 0), (unreal.OVAttackKind.MISSILE, 2), (unreal.OVAttackKind.GUIDED_MISSILE, 2)):
        combat, a, support, target = fixture(pc, 1)
        custom_profile(combat, a, index, kind=kind, speed=200, accuracy=1, salvo=1)
        combat.request_fire(a, weapon_id(combat, a, index))
        frames(pc, 3)
        shots = p(combat, 'attacks')
        check('Real projectile launched '+str(kind), len(shots) == 1 and p(shots[0], 'result') == unreal.OVAttackResult.FLYING)
        direction = p(shots[0], 'direction')
        direction = unreal.Vector(direction.x, direction.y, direction.z)
        frozen = p(shots[0], 'frozen_aim_point')
        frozen = unreal.Vector(frozen.x, frozen.y, frozen.z)
        control = p(shots[0], 'vls_control_point')
        control = unreal.Vector(control.x, control.y, control.z)
        faction = p(shots[0], 'faction_id')
        target.set_actor_location(unreal.Vector(1200, 400, 300), False, False)
        facility_disable(a, unreal.OVFacilityType.RADAR)
        p(a, 'systems').set_editor_property('faction_id', 1)
        p(a, 'systems').debug_destroy_slot(p(p(p(a, 'systems'), 'layout'), 'core_slot_id'), unreal.OVDamageCause.SABOTAGE)
        frames(pc, 12)
        shots = p(combat, 'attacks')
        check('Source loss preserves in-flight attack '+str(kind), len(shots) == 1 and p(shots[0], 'faction_id') == faction)
        changed = (p(shots[0], 'direction')-direction).length() > .001
        if kind == unreal.OVAttackKind.MISSILE:
            check('Ordinary VLS path stays frozen despite target motion', (p(shots[0], 'frozen_aim_point')-frozen).length() < .001 and (p(shots[0], 'vls_control_point')-control).length() < .001)
        else:
            check('Cannon stays straight and guided missile follows target '+str(kind), changed == (kind == unreal.OVAttackKind.GUIDED_MISSILE))
        if kind == unreal.OVAttackKind.GUIDED_MISSILE:
            combat.notify_ftl_departure(target); frames(pc, 3)
            check('Guided FTL departure is a distinct result', any(p(e, 'result') == unreal.OVAttackResult.TARGET_DEPARTED for e in p(combat, 'recent_events')))
    combat, a, support, target = fixture(pc, 1)
    custom_profile(combat, a, 2, salvo=3, salvo_interval=.18, speed=100, accuracy=1)
    combat.request_fire(a, weapon_id(combat, a, 2)); frames(pc, 3)
    facility_disable(a, unreal.OVFacilityType.MISSILE_STORAGE); frames(pc, 30)
    check('Supply loss cancels unlaunched salvo, preserves launched missile', p(combat, 'shot_count') == 1 and len(p(combat, 'attacks')) == 1 and p(mount(combat, a, 2), 'salvo_remaining') == 0)


def run_approved_policies(pc):
    for movement in (False, True):
        for attack in (False, True):
            combat, a, support, target = fixture(pc)
            combat.set_target(a, None)
            a.set_automation(movement, attack)
            frames(pc, 90)
            check('D101 independent automation '+str((movement, attack)), a.is_movement_automatic() == movement and a.is_attack_automatic() == attack and ((p(combat, 'shot_count') > 0) == attack) and p(a, 'hold'))
    combat, a, support, target = fixture(pc)
    target.set_actor_location(unreal.Vector(3000, 1000, 600), False, False)
    frames(pc, 300)
    known = p(mount(combat, a), 'last_tracked_world_point')
    known = unreal.Vector(known.x, known.y, known.z)
    facility_disable(a, unreal.OVFacilityType.RADAR)
    target.set_actor_location(unreal.Vector(3000, -1500, 600), False, False)
    frames(pc, 1)
    remaining = p(mount(combat, a), 'tracking_remaining')
    check('D084 loss starts 3.5 seconds even when no shot preceded loss', 3.4 < remaining <= 3.5 and p(combat, 'shot_count') == 0, remaining)
    pc.action('Fire'); frames(pc, 180)
    check('D084 hidden target motion cannot update last-known point or permit fire', (p(mount(combat, a), 'last_tracked_world_point')-known).length() < .001 and p(combat, 'shot_count') == 0)
    frames(pc, 180)
    check('D084 lost target tracking expires', p(mount(combat, a), 'tracking_remaining') == 0)
    combat, a, support, target = fixture(pc)
    st = p(target, 'systems'); layout = p(st, 'layout')
    mounts = list(p(layout, 'weapon_mounts'))
    mounts[0].set_editor_property('local_position', unreal.Vector(-210, 0, 8))
    layout.set_editor_property('weapon_mounts', mounts)
    st.initialize(layout)
    custom_profile(combat, a, damage=100, accuracy=1)
    hp = p(p(st, 'weapon_mount_states')[0], 'hp')
    hull = p(st, 'hull_hp'); shield = p(st, 'shield')
    pc.action('Fire'); frames(pc, 90)
    check('D089 real mount collision first consumes shield without mount damage', p(st, 'shield') < shield and p(p(st, 'weapon_mount_states')[0], 'hp') == hp)
    # Test precondition only: disable shield production by removing crew. The
    # following damage is still delivered exclusively by a launched beam.
    facility_disable(target, unreal.OVFacilityType.SHIELD_CONTROL)
    pc.action('Fire'); frames(pc, 90)
    check('D089 unshielded real collision damages one mount without hull transfer', p(p(st, 'weapon_mount_states')[0], 'hp') < hp and p(st, 'hull_hp') == hull and str(p(st, 'last_weapon_mount')) == str(p(mounts[0], 'weapon_id')) and p(st, 'last_hull_damage') == 0)


def stop():
    unreal.unregister_slate_post_tick_callback(state['handle'])


def tick(dt):
    try:
        if time.monotonic()-state['start'] > 180:
            raise TimeoutError('2B PIE API validation timeout')
        if state['phase'] == 0:
            check('Editor ControlRange loaded', editor.get_editor_world().get_name() == 'ControlRange')
            level.editor_request_begin_play(); state['phase'] = 1
            return
        if state['phase'] == 1:
            world = editor.get_game_world()
            if not world:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if not pc or len(p(pc, 'ships')) != 3:
                return
            check('Real PIE game world', level.is_in_play_in_editor() and 'UEDPIE' in world.get_path_name(), world.get_path_name())
            run_common(pc, world)
            state.update(phase=2, world=world)
            return
        if state['phase'] == 2:
            if time.monotonic()-state['at'] < 5:
                return
            combat, a, target = state['combat'], state['a'], state['target']
            shots_a = sum(p(m, 'shots_fired') for m in p(combat.get_ship_state(a), 'mounts'))
            shots_enemy = sum(p(m, 'shots_fired') for m in p(combat.get_ship_state(target), 'mounts'))
            check('Real PIE frames advance reciprocal player and enemy fire', p(combat, 'game_time') > .1 and shots_a > 0 and shots_enemy > 0, (p(combat, 'game_time'), shots_a, shots_enemy))
            pc = state['pc']; pause(pc, True)
            state.update(phase=3, at=time.monotonic(), shots=p(combat, 'shot_count'), clock=p(combat, 'game_time'))
            pc.select_ship(a); pc.action('External')
            return
        if state['phase'] == 3:
            if time.monotonic()-state['at'] < 1:
                return
            check('Rendered detail/HUD observes paused combat without extra shots', p(state['combat'], 'shot_count') == state['shots'] and p(state['combat'], 'game_time') == state['clock'])
            pc = state['pc']; pc.start_combat_range()
            pc.set_target(p(pc, 'ships')[2]); pc.action('External')
            move_window(pc, 10, 216)
            state.update(phase=4, at=time.monotonic())
            return
        if state['phase'] == 4:
            if time.monotonic()-state['at'] < 1:
                return
            hud = state['pc'].get_hud()
            check('Foreground detail window consumes clicks over underlying FIRE button', str(hud.get_hit_action_at_canvas_point(unreal.Vector2D(22,352))) == 'WindowBackground')
            move_window(state['pc'], 840, 76)
            state.update(phase=5, at=time.monotonic())
            return
        if state['phase'] == 5:
            if time.monotonic()-state['at'] < 1:
                return
            unreal.SystemLibrary.execute_console_command(state['world'], 'Shot SHOWUI', state['pc'])
            check('Fresh paused combat range ready for user input', p(state['pc'], 'simulation_paused') and p(p(state['pc'], 'combat'), 'shot_count') == 0)
            save(); stop(); unreal.log('OV2B VALIDATE COMPLETE')
    except Exception:
        report['errors'].append(traceback.format_exc())
        save(); unreal.log_error(report['errors'][-1]); stop()


state['handle'] = unreal.register_slate_post_tick_callback(tick)
