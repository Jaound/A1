"""BF120 원본 기반 시험 포탑의 F5 PIE 발사/피해/렌더 검사. 실제 입력이 아니다."""
import json, os, time, traceback
import unreal

saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
pc14_trial = os.environ.get('OV_BF120_PC14') == '1'
flush_trial = pc14_trial and os.environ.get('OV_BF120_FLUSH') == '1'
stem = 'BF120PC14FlushTrial' if flush_trial else 'BF120PC14Trial' if pc14_trial else 'BF120LiveTrial'
out = os.path.join(saved, 'Validation', stem + '.json')
png = os.path.join(saved, 'Validation', stem + '.png').replace('\\', '/')
os.makedirs(os.path.dirname(out), exist_ok=True)
report = {'passed': None, 'physical_input': False, 'scope': ('F6 PC14' if pc14_trial else 'F5') + ' PIE API 시험 프로필, 실제 입력 및 기본 모델 적용 아님', 'checks': [], 'errors': [], 'image': png}
state = {'phase': 'start', 'started': time.monotonic()}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
def p(obj, key): return obj.get_editor_property(key)
def check(key, condition, detail=''):
    report['checks'].append({'id': key, 'passed': bool(condition), 'detail': str(detail)})
    if not condition: raise AssertionError(key + ': ' + str(detail))
def finish():
    if state.get('floating'):
        report['play_settings_restored'] = bool(unreal.OVDevelopmentEditorLibrary.restore_preview_settings())
    report['passed'] = bool(report['checks']) and all(c['passed'] for c in report['checks']) and not report['errors']
    with open(out, 'w', encoding='utf-8') as f: json.dump(report, f, ensure_ascii=False, indent=2)
    unreal.unregister_slate_post_tick_callback(state['handle'])
def tick(_):
    try:
        if time.monotonic() - state['started'] > 90: raise RuntimeError('PIE 검사 시간 초과')
        if state['phase'] == 'start':
            if os.path.exists(png): os.remove(png)
            width, height = (int(v) for v in os.environ.get('OV_UI_CAPTURE_RES','1280x720').split('x'))
            if not unreal.OVDevelopmentEditorLibrary.start_preview_window(width,height): raise RuntimeError('별도 플레이 창 시작 실패')
            state.update(phase='wait',floating=True); return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(p(pc, 'ships')) < 3: return
        if state['phase'] == 'wait':
            if pc14_trial:
                unreal.SystemLibrary.execute_console_command(world,'ov.Presentation.ImportedHull 1',pc)
                check('PC14_SCENE',pc.start_calibration_range(1))
            else: pc.start_combat_range()
            ships = list(p(pc, 'ships')); source, target = ships[1], ships[2]
            for ship in ships:
                ship.set_automation(False, False); ship.stop_movement(); ship.debug_reset_motion()
                ship.set_actor_rotation(unreal.Rotator(), False)
            source.set_actor_location(unreal.Vector(0,0,300),False,False)
            target.set_actor_location(unreal.Vector(12000 if pc14_trial else 1200,0,300),False,False)
            ships[0].set_actor_location(unreal.Vector(-15000,10000,300),False,False)
            systems=p(source,'systems'); layout=p(systems,'layout')
            check('TRANSIENT_LAYOUT', not layout.get_path_name().startswith('/Game/Layouts/'),layout.get_path_name())
            definitions=list(p(layout,'weapon_mounts'))
            index=next(i for i,d in enumerate(definitions) if p(d,'type')==unreal.OVWeaponType.CANNON)
            definition=definitions[index]; mount_id=p(definition,'weapon_id')
            if flush_trial:
                # Trial layout only: place the measured base underside at the sampled PC14 top surface.
                # This also moves the real muzzle; it is NOT a production hardpoint change.
                mesh=unreal.load_asset('/Game/Presentation/BF120Trial/SM_BF120_BaseTrial')
                bounds=mesh.get_bounding_box(); extent=p(layout,'local_half_extent'); tier=p(definition,'tier')
                length=(26.0+max(0.0,tier-1.0)*3.0)*max(1.0,min(12.0,min(extent.x,extent.y,extent.z)/30.0))
                lower_offset=(bounds.min.z+41.0)*length/249.143
                position=p(definition,'local_position'); original_z=position.z; position.z=-2.5506140066-lower_offset
                definition.set_editor_property('local_position',position)
                report['trial_hardpoint']={'original_z_cm':original_z,'candidate_z_cm':position.z,'base_lower_offset_cm':lower_offset,'surface_z_cm':-2.5506140066}
                for component in source.get_components_by_class(unreal.StaticMeshComponent):
                    if component.get_name().startswith('PC14MountSupport_'): component.set_visibility(False,True)
            definition.set_editor_property('presentation_profile','BF120Trial'); definitions[index]=definition
            layout.set_editor_property('weapon_mounts',definitions)
            check('OPT_IN_PROFILE',str(p(p(layout,'weapon_mounts')[index],'presentation_profile'))=='BF120Trial')
            combat=p(pc,'combat'); settings=p(combat,'settings')
            mount=next(m for m in p(combat.get_ship_state(source),'mounts') if p(m,'mount_id')==mount_id)
            profile=p(mount,'profile'); profile.set_editor_property('profile_id','BF120LiveTest')
            for name,value in [('accuracy',1.0),('aim_spread_degrees',0.0),('fire_tolerance',180.0),('range',100000.0),('salvo',1)]: profile.set_editor_property(name,value)
            profiles=list(p(settings,'weapons')); profiles.append(profile); settings.set_editor_property('weapons',profiles)
            check('PROFILE_REGISTERED',combat.set_weapon_profile(source,mount_id,'BF120LiveTest'))
            check('TARGET_SET',combat.set_target(source,target))
            p(settings,'guaranteed_range')
            settings.set_editor_property('guaranteed_range',100000.0)
            source.refresh_weapon_visuals(); pc.select_ship(source)
            pc.set_editor_property('camera_focus',source.get_actor_location()); pc.set_editor_property('camera_distance',4000 if pc14_trial else 700)
            pc.set_editor_property('camera_yaw',-45); pc.set_editor_property('camera_pitch',-30); pc.update_camera()
            combat.simulate(1.0)
            before=p(p(target,'systems'),'shield'); shots=p(combat,'shot_count'); hits=p(combat,'hit_count')
            check('REQUEST',combat.request_fire(source,mount_id))
            combat.simulate(1/120)
            check('ONE_SHOT',p(combat,'shot_count')==shots+1)
            attack=list(p(combat,'attacks'))[-1]
            visual=next(v for v in source.get_weapon_visual_states() if p(v,'mount_id')==mount_id)
            check('ACTUAL_ORIGIN',(p(attack,'origin')-p(visual,'world_muzzle')).length()<.1)
            if flush_trial:
                origin=p(attack,'origin'); center=source.get_actor_location(); scale=source.get_actor_scale3d()
                local_z=(origin.z-center.z)/scale.z
                report['trial_hardpoint']['firing_origin_local_z_cm']=local_z
                report['trial_hardpoint']['combat_obb_half_height_cm']=extent.z
                report['trial_hardpoint']['origin_inside_combat_obb_height']=abs(local_z)<extent.z
                radius=p(settings,'mount_collision_radius')
                report['trial_hardpoint']['mount_proxy_radius_cm']=radius
                report['trial_hardpoint']['mount_proxy_top_cm']=position.z+radius
                report['trial_hardpoint']['proxy_below_obb_top']=position.z+radius<extent.z
            check('FLASH',p(visual,'flash_visible'))
            combat.simulate(.5)
            check('HIT',p(combat,'hit_count')>hits,(p(combat,'hit_count'),hits))
            check('SHIELD_DAMAGE',p(p(target,'systems'),'shield')<before,(before,p(p(target,'systems'),'shield')))
            state.update(phase='warmup',at=time.monotonic()); return
        if state['phase']=='warmup' and time.monotonic()-state['at']>1:
            unreal.SystemLibrary.execute_console_command(world,'Shot SHOWUI filename="'+png+'" -nosuffix',pc)
            state.update(phase='capture',at=time.monotonic()); return
        if state['phase']=='capture' and time.monotonic()-state['at']>2:
            check('RENDER_FILE',os.path.isfile(png)); finish()
    except Exception:
        report['errors'].append(traceback.format_exc()); unreal.log_error(report['errors'][-1]); finish()
state['handle']=unreal.register_slate_post_tick_callback(tick)
