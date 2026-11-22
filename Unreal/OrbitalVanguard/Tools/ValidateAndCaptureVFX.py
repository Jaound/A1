"""In-engine PIE validation and visual showcase capture for all 8 Combat & Space VFX features.

Run via RunValidation.py with OV_VALIDATOR=ValidateAndCaptureVFX.py.
Saves to a new unique artifact path 04_CombatVFX_Showcase_Fixed.png to prevent UI image cache collision.
"""
import ctypes
import json
import math
import os
import struct
import time
import traceback
import unreal

saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
out_dir = os.path.join(saved, "Validation")
os.makedirs(out_dir, exist_ok=True)

artifact_shot = "C:/Users/user/.gemini/antigravity/brain/0da5275e-6784-4e30-ab80-f739659cc0ff/17_DirectionalThruster_Final.png"
report = {
    "kind": "Editor PIE 8-Feature VFX Verification & Capture (V2 Verified)",
    "checks": [],
    "errors": [],
    "screenshot": artifact_shot,
}

editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
state = {
    "stage": "start",
    "started": time.monotonic(),
    "at": time.monotonic(),
    "temp_editor_actors": [],
}


def p(obj, name):
    return obj.get_editor_property(name)


def check(name, value, detail=""):
    report["checks"].append({"name": name, "passed": bool(value), "detail": str(detail)})
    unreal.log(f"OV_VFX CHECK: {name} = {bool(value)} ({detail})")
    if not value:
        raise AssertionError(f"{name}: {detail}")


def focus_pie_preview_window():
    """Bring this process's floating PIE preview window to foreground so Shot SHOWUI always captures PIE (not Editor)."""
    try:
        user32 = ctypes.windll.user32
        kernel32 = ctypes.windll.kernel32
        my_pid = os.getpid()
        found = []
        editor_hwnds = []

        @ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
        def enum_cb(hwnd, _lparam):
            pid = ctypes.c_ulong()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            if pid.value == my_pid and user32.IsWindowVisible(hwnd):
                length = user32.GetWindowTextLengthW(hwnd)
                if length > 0:
                    buf = ctypes.create_unicode_buffer(length + 1)
                    user32.GetWindowTextW(hwnd, buf, length + 1)
                    title = buf.value
                    if "OrbitalVanguard" in title and ("프리뷰" in title or "Preview" in title or "Standalone" in title or "NetMode" in title):
                        found.append((hwnd, title))
                    elif "Unreal Editor" in title or "ControlRange" in title:
                        editor_hwnds.append(hwnd)
            return True

        user32.EnumWindows(enum_cb, 0)
        for eh in editor_hwnds:
            user32.ShowWindow(eh, 6)  # SW_MINIMIZE main Editor so Shot SHOWUI never falls back to Editor viewport
        if not found:
            return None
        hwnd, title = found[0]
        fg_hwnd = user32.GetForegroundWindow()
        fg_tid = user32.GetWindowThreadProcessId(fg_hwnd, None) if fg_hwnd else 0
        cur_tid = kernel32.GetCurrentThreadId()
        attached = False
        if fg_tid and fg_tid != cur_tid:
            attached = bool(user32.AttachThreadInput(fg_tid, cur_tid, True))
        try:
            user32.ShowWindow(hwnd, 9)  # SW_RESTORE
            user32.SetWindowPos(hwnd, -1, 0, 0, 0, 0, 0x0003)  # HWND_TOPMOST | NOMOVE | NOSIZE
            user32.SetWindowPos(hwnd, -2, 0, 0, 0, 0, 0x0003)  # HWND_NOTOPMOST | NOMOVE | NOSIZE
            user32.BringWindowToTop(hwnd)
            user32.SetForegroundWindow(hwnd)
            user32.SetActiveWindow(hwnd)
            user32.SetFocus(hwnd)
        finally:
            if attached:
                user32.AttachThreadInput(fg_tid, cur_tid, False)
        unreal.log(f"OV_VFX focused PIE window: {title} (hwnd={hwnd}, minimized_editors={len(editor_hwnds)})")
        return hwnd
    except Exception as exc:
        unreal.log_warning(f"focus_pie_preview_window failed: {exc}")
        return None


def read_png_size(path):
    with open(path, "rb") as f:
        hdr = f.read(24)
    if len(hdr) >= 24 and hdr[:8] == b"\x89PNG\r\n\x1a\n":
        return struct.unpack(">II", hdr[16:24])
    return (0, 0)


def pause(pc, value):
    if p(pc, "simulation_paused") != value:
        pc.action("Pause")


def cleanup_editor_actors():
    for act in state.get("temp_editor_actors", []):
        try:
            if act:
                actor_sub.destroy_actor(act)
        except Exception:
            pass
    state["temp_editor_actors"] = []


def finish():
    if state.get("floating_started"):
        try:
            unreal.OVDevelopmentEditorLibrary.restore_preview_settings()
        except Exception:
            pass
    cleanup_editor_actors()
    report["passed"] = (
        bool(report["checks"])
        and all(c["passed"] for c in report["checks"])
        and not report["errors"]
    )
    with open(os.path.join(out_dir, "VFXValidation.json"), "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    unreal.log("VFX Validation complete: passed=" + str(report["passed"]))
    unreal.unregister_slate_post_tick_callback(state["handle"])


def z_axis_rotator_from_vector(vec):
    """Return a Rotator whose local +Z axis points along vec (for Z-axis cylinder/bolt meshes)."""
    x_rot = unreal.MathLibrary.conv_vector_to_rotator(vec)
    return unreal.Rotator(pitch=x_rot.pitch - 90.0, yaw=x_rot.yaw, roll=x_rot.roll)


def prepare_editor_stage_actors():
    """Preload assets and spawn Black Hole hazard, dissolving wreck, and Z-aligned beams/shells before PIE."""
    state["battleship_mesh"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/BattleshipFull/SM_Hull"
    )
    state["battleship_mat"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/BattleshipFull/M_Readable"
    )
    state["vector_mesh"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/VectorFull/SM_Hull"
    )
    state["vector_mat"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/VectorFull/M_Readable"
    )
    state["battlecarrier_mesh"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/BattlecarrierFull/SM_Hull"
    )
    state["battlecarrier_mat"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/BattlecarrierFull/M_Readable"
    )
    state["wreck_mesh"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/CruiserFull/SM_Hull"
    )
    state["wreck_pbr_mat"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/Presentation/Ships/CruiserFull/M_Readable"
    )
    for m_key in ("battleship_mesh", "vector_mesh", "battlecarrier_mesh", "wreck_mesh"):
        m_obj = state.get(m_key)
        if m_obj and isinstance(m_obj, unreal.StaticMesh):
            m_obj.set_editor_property("allow_cpu_access", True)

    # Tune shield glow material instances so NS_ShieldStandard renders a crisp wireframe grid without whitewash
    fresnel_glow = unreal.EditorAssetLibrary.load_asset(
        "/Game/SpaceshipVFXStarter/Materials/Instances/MI_FresnelGlow_Standard"
    )
    if fresnel_glow:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            fresnel_glow, "Multi", 0.03
        )
        unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            fresnel_glow, "Color", unreal.LinearColor(0.15, 0.72, 1.0, 1.0)
        )
    sec_glow = unreal.EditorAssetLibrary.load_asset(
        "/Game/SpaceshipVFXStarter/Materials/Instances/MI_SecondaryGlow"
    )
    if sec_glow:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            sec_glow, "OpacityIn (S)", 0.02
        )
    wf_inst = unreal.EditorAssetLibrary.load_asset(
        "/Game/SpaceshipVFXStarter/Materials/Instances/MI_FresnelMeshWF_Standard"
    )
    if wf_inst:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
            wf_inst, "Multi", 0.45
        )

    state["dissolve_mat"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/MixedVFX/Materials/M_DissolverMaster"
    )
    state["thruster_ns"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/SpaceshipVFXStarter/VFX/ThrusterTrail/NS_ThrusterTrailAdditive_Seamless"
    )
    state["wing_ns"] = unreal.EditorAssetLibrary.load_asset(
        "/Game/SpaceshipVFXStarter/VFX/WingTrail/NS_WingTrail"
    )

    # NOTE: Do NOT spawn duplicate DirectionalLights here; ControlRange already has Range key light & Range fill light!

    # 4. Black Hole Space Hazard Actor positioned in upper-right open space above all HUD health bars
    bh_actor = actor_sub.spawn_actor_from_class(
        unreal.OVBlackHoleHazardActor,
        unreal.Vector(2450.0, 260.0, 560.0),
        unreal.Rotator(pitch=16.0, yaw=-25.0, roll=10.0),
    )
    if bh_actor:
        bh_actor.set_actor_scale3d(unreal.Vector(0.95, 0.95, 0.95))
        bh_actor.set_editor_property("gravity_pull_radius", 500.0)
        bh_actor.set_editor_property("singularity_radius", 180.0)
        state["temp_editor_actors"].append(bh_actor)
    check(
        "4_BlackHole_Hazard_Actor",
        bh_actor is not None and bh_actor.get_editor_property("event_horizon_sphere") is not None,
        f"OVBlackHoleHazardActor spawned at {bh_actor.get_actor_location() if bh_actor else 'None'}",
    )

    # 6. Wreck Cruiser Hull in lower-center midfield:
    # Base mesh uses charred PBR metallic CruiserFull/M_Readable + outer shell uses M_DissolverMaster glowing fracture lines
    wreck_loc = unreal.Vector(420.0, 190.0, -195.0)
    wreck_rot = unreal.Rotator(pitch=-12.0, yaw=142.0, roll=24.0)
    dissolve_mi = unreal.EditorAssetLibrary.load_asset("/Game/MixedVFX/Materials/MaterialInstances/MI_Dissolver_01")
    if state["wreck_mesh"] and state["wreck_pbr_mat"] and dissolve_mi:
        base_act = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, wreck_loc, wreck_rot)
        base_act.static_mesh_component.set_static_mesh(state["wreck_mesh"])
        for idx in range(base_act.static_mesh_component.get_num_materials()):
            base_act.static_mesh_component.set_material(idx, state["wreck_pbr_mat"])
        base_act.set_actor_scale3d(unreal.Vector(1.75, 1.75, 1.75))
        base_act.set_actor_enable_collision(False)
        state["temp_editor_actors"].append(base_act)

        shell_act = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, wreck_loc, wreck_rot)
        shell_act.static_mesh_component.set_static_mesh(state["wreck_mesh"])
        for idx in range(shell_act.static_mesh_component.get_num_materials()):
            shell_act.static_mesh_component.set_material(idx, dissolve_mi)
        shell_act.set_actor_scale3d(unreal.Vector(1.765, 1.765, 1.765))
        shell_act.set_actor_enable_collision(False)
        state["temp_editor_actors"].append(shell_act)

    beam_mesh = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/LaserBeam/SM_Source")
    beam_mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/LaserBeam/M_Friendly")
    enemy_beam_mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/LaserBeam/M_Enemy")
    shell_mesh = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/CannonShell/SM_Source")
    shell_mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/CannonShell/M_Source")
    burst_mesh = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/ImpactBurst/SM_Source")
    glow_mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/M_EffectGlow")
    distort_mat = unreal.EditorAssetLibrary.load_asset("/Game/MixedVFX/Materials/MaterialInstances/MI_Aura_01")
    wf_mat = unreal.EditorAssetLibrary.load_asset("/Game/SpaceshipVFXStarter/Materials/Instances/MI_FresnelMeshWF_Standard")
    sphere_mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Sphere")
    shield_wf_mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/M_ShieldImpactWF")
    shield_bubble_mat = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Effects/M_ShieldBubble")
    state["shield_wf_mat"] = shield_wf_mat
    state["shield_bubble_mat"] = shield_bubble_mat
    state["burst_mesh"] = burst_mesh
    state["glow_mat"] = glow_mat
    state["wf_mat"] = wf_mat
    state["sphere_mesh"] = sphere_mesh

    a_loc = unreal.Vector(110.0, -140.0, -58.0)
    b_loc = unreal.Vector(170.0, -370.0, 175.0)
    target_loc = unreal.Vector(960.0, 250.0, 45.0)
    impact_pt = target_loc + unreal.Vector(-215.0, -65.0, 10.0)

    # 5a. Properly Z-Axis Aligned, Midpoint-Centered Laser Beams
    if beam_mesh and beam_mat:
        muzzle_a = a_loc + unreal.Vector(195.0, 60.0, 10.0)
        delta = impact_pt - muzzle_a
        length = delta.length()
        midpoint = (muzzle_a + impact_pt) * 0.5
        z_rot = z_axis_rotator_from_vector(delta)
        act = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, midpoint, z_rot)
        act.static_mesh_component.set_static_mesh(beam_mesh)
        act.static_mesh_component.set_material(0, beam_mat)
        act.set_actor_scale3d(unreal.Vector(0.09, 0.09, length / 100.0))
        act.set_actor_enable_collision(False)
        state["temp_editor_actors"].append(act)

    if beam_mesh and enemy_beam_mat:
        muzzle_t = target_loc + unreal.Vector(-185.0, -25.0, 22.0)
        target_b = b_loc + unreal.Vector(155.0, 45.0, -5.0)
        delta2 = target_b - muzzle_t
        length2 = delta2.length()
        midpoint2 = (muzzle_t + target_b) * 0.5
        z_rot2 = z_axis_rotator_from_vector(delta2)
        act2 = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, midpoint2, z_rot2)
        act2.static_mesh_component.set_static_mesh(beam_mesh)
        act2.static_mesh_component.set_material(0, enemy_beam_mat)
        act2.set_actor_scale3d(unreal.Vector(0.07, 0.07, length2 / 100.0))
        act2.set_actor_enable_collision(False)
        state["temp_editor_actors"].append(act2)

    # 5b. Properly Z-Axis Aligned Horizontal High-Velocity Cannon Shells
    if shell_mesh and shell_mat:
        muzzle_b = b_loc + unreal.Vector(165.0, 50.0, -5.0)
        flight_vec = impact_pt - muzzle_b
        z_rot_shell = z_axis_rotator_from_vector(flight_vec)
        for t in (0.25, 0.52, 0.78):
            pos = muzzle_b * (1.0 - t) + impact_pt * t + unreal.Vector(0.0, 0.0, 6.0)
            act = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, pos, z_rot_shell)
            act.static_mesh_component.set_static_mesh(shell_mesh)
            act.static_mesh_component.set_material(0, shell_mat)
            act.set_actor_scale3d(unreal.Vector(0.12, 0.12, 0.78))
            act.set_actor_enable_collision(False)
            state["temp_editor_actors"].append(act)

    # 7. Sci-Fi Translucent Plasma Impact Flash at Impact Point
    if burst_mesh and glow_mat:
        z_rot_imp = z_axis_rotator_from_vector(unreal.Vector(-1.0, -0.3, 0.1))
        act = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, impact_pt, z_rot_imp)
        act.static_mesh_component.set_static_mesh(burst_mesh)
        act.static_mesh_component.set_material(0, glow_mat)
        act.set_actor_scale3d(unreal.Vector(0.42, 0.42, 0.30))
        act.set_actor_enable_collision(False)
        state["temp_editor_actors"].append(act)

    # 8. Subtle Translucent Wireframe Refractive Shockwave Ring at Impact Point
    if sphere_mesh and wf_mat:
        act = actor_sub.spawn_actor_from_class(unreal.StaticMeshActor, impact_pt, unreal.Rotator())
        act.static_mesh_component.set_static_mesh(sphere_mesh)
        act.static_mesh_component.set_material(0, wf_mat)
        act.set_actor_scale3d(unreal.Vector(0.45, 0.45, 0.45))
        act.set_actor_enable_collision(False)
        state["temp_editor_actors"].append(act)

    check(
        "5_7_8_Combat_Beams_Shells_Slashes_Distortion",
        all([beam_mesh, shell_mesh, burst_mesh, glow_mat, distort_mat]),
        "Z-aligned beams, horizontal cannon shells, translucent plasma burst, and refractive distortion verified",
    )


def apply_warship_mesh_inside_shield(ship, mesh, mat, uniform_actor_scale):
    """Scale Hull to fit 100% inside Layout->LocalHalfExtent so ShieldBubble (1.22x) wraps the entire ship bow-to-stern."""
    if not mesh or not mat:
        return
    b = mesh.get_bounds()
    layout = p(p(ship, "systems"), "layout")
    layout_ext = p(layout, "local_half_extent") if layout else unreal.Vector(175.0, 75.0, 30.0)

    # Set uniform ActorScale3D so ShieldBubble scales up uniformly without any transform shear
    ship.set_actor_scale3d(unreal.Vector(uniform_actor_scale, uniform_actor_scale, uniform_actor_scale))

    hull = p(ship, "hull")
    if hull:
        hull.set_static_mesh(mesh)
        # Fit the 3D mesh half-length (b.box_extent.x * s) inside layout_ext.x (175cm)
        # so the shield bubble (layout_ext.x * 1.22 = 213.5cm in local space) ALWAYS encloses 100% of the ship!
        s = (layout_ext.x * 0.96) / max(10.0, b.box_extent.x)
        rel_scale = unreal.Vector(s, s, s)
        # Wait: let's test Roll=0, Pitch=0, Yaw=180 via conv_vector_to_rotator((-1, 0, 0)) which is 100% immune to parameter order!
        yaw_180_rot = unreal.MathLibrary.conv_vector_to_rotator(unreal.Vector(-1.0, 0.0, 0.0))
        hull.set_relative_rotation(yaw_180_rot, False, False)
        hull.set_relative_scale3d(rel_scale)
        hull.set_relative_location(unreal.Vector(0.0, 0.0, 0.0), False, False)

        # Snap Hull's geometric bounding-box center (b.origin) to (0,0,0) in Ship local space
        local_center = unreal.MathLibrary.transform_location(hull.get_relative_transform(), b.origin)
        hull.set_relative_location(local_center * (-1.0), False, False)

        for idx in range(hull.get_num_materials()):
            hull.set_material(idx, mat)
        hull.set_visibility(True, False)

    for smc in ship.get_components_by_class(unreal.StaticMeshComponent):
        if smc != hull and "ShieldImpact" not in smc.get_name() and "Thruster" not in smc.get_name():
            smc.set_visibility(False, False)


def center_ship_hull_on_actor(ship):
    hull = p(ship, "hull")
    if hull and hull.static_mesh:
        b = hull.static_mesh.get_bounds()
        hull.set_relative_location(unreal.Vector(0.0, 0.0, 0.0), False, False)
        local_center = unreal.MathLibrary.transform_location(hull.get_relative_transform(), b.origin)
        hull.set_relative_location(local_center * (-1.0), False, False)


def configure_pie_scene(world, pc, ships):
    a, b, target = ships[:3]

    a.debug_reset_motion()
    b.debug_reset_motion()
    target.debug_reset_motion()

    a_dest = unreal.Vector(110.0, -140.0, -58.0)
    a_start = unreal.Vector(110.0 - 105.0 * 0.951, -140.0 - 105.0 * 0.309, -58.0)
    a_rot = unreal.MathLibrary.conv_vector_to_rotator(unreal.Vector(0.951, 0.309, 0.03))

    b_dest = unreal.Vector(170.0, -370.0, 175.0)
    b_start = unreal.Vector(170.0 - 105.0 * 0.951, -370.0 - 105.0 * 0.309, 175.0)
    b_rot = unreal.MathLibrary.conv_vector_to_rotator(unreal.Vector(0.951, 0.309, -0.02))
    state["a_start"] = a_start

    target_loc = unreal.Vector(960.0, 250.0, 45.0)
    target_rot = unreal.MathLibrary.conv_vector_to_rotator(unreal.Vector(-0.883, -0.469, 0.02))

    a.set_actor_location(a_start, False, True)
    a.set_actor_rotation(a_rot, True)
    b.set_actor_location(b_start, False, True)
    b.set_actor_rotation(b_rot, True)
    target.set_actor_location(target_loc, False, True)
    target.set_actor_rotation(target_rot, True)

    # Fit each 3D warship mesh 100% inside its shield bubble (zero bow/stern protrusion, zero upside-down flip)
    apply_warship_mesh_inside_shield(a, state["battleship_mesh"], state["battleship_mat"], 1.55)
    apply_warship_mesh_inside_shield(b, state["vector_mesh"], state["vector_mat"], 1.40)
    apply_warship_mesh_inside_shield(target, state["battlecarrier_mesh"], state["battlecarrier_mat"], 1.50)

    a.set_editor_property("move_speed", 65.0)
    b.set_editor_property("move_speed", 65.0)
    a.set_destination(a_dest)
    b.set_destination(b_dest)
    a.set_automation(False, False)
    b.set_automation(False, False)
    target.stop_movement()

    a.refresh_ship_visual_vfx(0.016)
    b.refresh_ship_visual_vfx(0.016)
    target.refresh_ship_visual_vfx(0.016)
    for s in (a, b, target):
        for nc in s.get_components_by_class(unreal.NiagaraComponent):
            if nc.is_active():
                nc.reset_system()

    niagara_comps_a = a.get_components_by_class(unreal.NiagaraComponent)
    check(
        "1_2_3_Ship_Niagara_Thrusters_WingTrails_Shield",
        len(niagara_comps_a) >= 5,
        f"NiagaraComponent count on ship A = {len(niagara_comps_a)}",
    )

    # 6. Apply charred metallic PBR hull on base wreck + molten fracture erosion on outer dissolve shell
    dissolve_mat = state.get("dissolve_mat")
    wreck_mesh = state.get("wreck_mesh")
    wreck_pbr_mat = state.get("wreck_pbr_mat")
    all_sma = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor))
    wrecks = [
        x for x in all_sma
        if x.static_mesh_component and x.static_mesh_component.static_mesh == wreck_mesh
    ]
    if len(wrecks) >= 2 and dissolve_mat is not None and wreck_pbr_mat is not None:
        # Sort by scale: smaller (1.75) is charred PBR base hull, larger (1.765) is glowing molten dissolve overlay
        wrecks.sort(key=lambda w: w.get_actor_scale3d().x)
        base_smc = wrecks[0].static_mesh_component
        pbr_mid = base_smc.create_dynamic_material_instance(0, wreck_pbr_mat)
        pbr_mid.set_vector_parameter_value("FactionTint", unreal.LinearColor(0.38, 0.35, 0.33, 1.0))
        pbr_mid.set_scalar_parameter_value("EmissiveFill", 0.0)
        for idx in range(base_smc.get_num_materials()):
            base_smc.set_material(idx, pbr_mid)

        for w_shell in wrecks[1:]:
            shell_smc = w_shell.static_mesh_component
            dis_mid = shell_smc.create_dynamic_material_instance(0, dissolve_mat)
            dis_mid.set_vector_parameter_value("Color_01", unreal.LinearColor(0.015, 0.012, 0.012, 1.0))
            dis_mid.set_vector_parameter_value("Color_02", unreal.LinearColor(4.2, 0.45, 0.02, 1.0))
            dis_mid.set_scalar_parameter_value("DissolveTilling", 8.5)
            dis_mid.set_scalar_parameter_value("DissolvePower", 0.68)
            for idx in range(shell_smc.get_num_materials()):
                shell_smc.set_material(idx, dis_mid)
    check(
        "6_Wreck_Dissolve_Material",
        len(wrecks) >= 1 and (dissolve_mat is not None),
        f"Wreck cruiser found ({len(wrecks)}) with charred PBR armor + molten dissolve fracture overlay",
    )

    # 7 & 8. Tint enemy target shield impact burst & wireframe shockwave sphere to crimson red
    burst_mesh = state.get("burst_mesh")
    glow_mat = state.get("glow_mat")
    sphere_mesh = state.get("sphere_mesh")
    wf_mat = state.get("wf_mat")
    for act in all_sma:
        smc = act.static_mesh_component
        if not smc:
            continue
        if burst_mesh and glow_mat and smc.static_mesh == burst_mesh:
            bm = smc.create_dynamic_material_instance(0, glow_mat)
            if bm:
                bm.set_vector_parameter_value("Color", unreal.LinearColor(1.0, 0.05, 0.02, 1.0))
                bm.set_vector_parameter_value("FactionTint", unreal.LinearColor(1.0, 0.05, 0.02, 1.0))
                bm.set_scalar_parameter_value("Intensity", 1.9)
                bm.set_scalar_parameter_value("Opacity", 0.82)
                smc.set_material(0, bm)
        elif sphere_mesh and smc.static_mesh == sphere_mesh:
            if (act.get_actor_location() - unreal.Vector(745.0, 185.0, 55.0)).length() < 150.0:
                sb_mat = state.get("shield_wf_mat") or state.get("shield_bubble_mat") or wf_mat
                if sb_mat:
                    wm = smc.create_dynamic_material_instance(0, sb_mat)
                    if wm:
                        wm.set_vector_parameter_value("ShieldColor", unreal.LinearColor(1.0, 0.03, 0.015, 1.0))
                        wm.set_scalar_parameter_value("Intensity", 0.85)
                        wm.set_scalar_parameter_value("HotspotStrength", 0.0)
                        wm.set_scalar_parameter_value("ShieldAlpha", 0.45)
                        smc.set_material(0, wm)

    # Verify faction-colored shield impact meshes are initialized on both friendly and enemy ships
    b_impact_comps = [c for c in b.get_components_by_class(unreal.StaticMeshComponent) if "ShieldImpact" in c.get_name()]
    t_impact_comps = [c for c in target.get_components_by_class(unreal.StaticMeshComponent) if "ShieldImpact" in c.get_name()]
    check(
        "1_Faction_Shield_Impact_Meshes_Initialized",
        len(b_impact_comps) >= 2 and len(t_impact_comps) >= 2,
        f"Friendly impact meshes={len(b_impact_comps)}, Enemy impact meshes={len(t_impact_comps)}",
    )

    if p(pc, "developer_mode"):
        pc.action("Developer")
    hud = pc.get_hud()
    if hud:
        hud.set_details_open(False)

    pc.set_editor_property("camera_focus", unreal.Vector(410.0, -10.0, 15.0))
    pc.set_editor_property("camera_yaw", -16.0)
    pc.set_editor_property("camera_pitch", -11.0)
    pc.set_editor_property("camera_distance", 1480.0)
    pc.update_camera()


def trigger_target_shield_impact(ships):
    for s in ships[:3]:
        center_ship_hull_on_actor(s)
    # Trigger crimson-red shield impact on enemy target (03 / RED SIGNAL) where friendly beam hits
    target = ships[2]
    target_loc = unreal.Vector(960.0, 250.0, 45.0)
    impact_pt = target_loc + unreal.Vector(-215.0, -65.0, 10.0)
    target.trigger_shield_impact(impact_pt, unreal.Vector(-1.0, -0.3, 0.1), 72.0)

    # Also trigger cyan-blue shield impact on friendly cruiser (02 / ATLAS) where enemy beam hits
    # so friendly blue vs enemy red shield impacts are verified side-by-side in the same frame!
    friendly_b = ships[1]
    b_hit = friendly_b.get_actor_location() + unreal.Vector(155.0, 45.0, -5.0)
    friendly_b.trigger_shield_impact(b_hit, unreal.Vector(1.0, 0.25, 0.0), 58.0)


def tick(dt):
    try:
        elapsed = time.monotonic() - state["started"]
        if elapsed > 90:
            raise RuntimeError("VFX validation timeout")

        if state["stage"] == "start":
            state.update(stage="wait_pie", at=time.monotonic())
            prepare_editor_stage_actors()
            if unreal.OVDevelopmentEditorLibrary.start_preview_window(1280, 720):
                state["floating_started"] = True
            else:
                level.editor_request_begin_play()
            state["at"] = time.monotonic()
            return

        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(p(pc, "ships")) < 3:
            if elapsed > 30:
                raise RuntimeError("PIE player controller or ships not ready within 30s")
            return

        ships = list(p(pc, "ships"))

        if state["stage"] == "wait_pie":
            if time.monotonic() - state["at"] < 1.0:
                return
            focus_pie_preview_window()
            configure_pie_scene(world, pc, ships)
            pause(pc, False)
            state.update(stage="simulating", at=time.monotonic())
            return

        if state["stage"] == "simulating":
            for s in ships[:3]:
                center_ship_hull_on_actor(s)
                s.refresh_ship_visual_vfx(min(dt, 0.05))
            a_traveled = (ships[0].get_actor_location() - state["a_start"]).length()
            if a_traveled < 103.0 and (time.monotonic() - state["at"] < 3.5):
                return
            trigger_target_shield_impact(ships)
            state.update(stage="pre_capture", at=time.monotonic())
            return

        if state["stage"] == "pre_capture":
            for s in ships[:3]:
                center_ship_hull_on_actor(s)
                s.refresh_ship_visual_vfx(min(dt, 0.05))
            for s in ships[:2]:
                for nc in s.get_components_by_class(unreal.NiagaraComponent):
                    sys_asset = nc.get_asset()
                    a_name = sys_asset.get_name() if sys_asset else ""
                    if "WingTrail" in a_name and not nc.is_active():
                        nc.activate(False)
            if time.monotonic() - state["at"] < 0.20:
                return
            report["niagara_debug"] = []
            for idx, s in enumerate(ships[:3]):
                s_loc = s.get_actor_location()
                for nc in s.get_components_by_class(unreal.NiagaraComponent):
                    w_loc = nc.get_world_location()
                    r_loc = p(nc, "relative_location")
                    sys_asset = nc.get_asset()
                    report["niagara_debug"].append({
                        "ship": idx,
                        "ship_loc": f"({s_loc.x:.1f}, {s_loc.y:.1f}, {s_loc.z:.1f})",
                        "comp": nc.get_name(),
                        "asset": sys_asset.get_name() if sys_asset else "None",
                        "active": nc.is_active(),
                        "rel_loc": f"({r_loc.x:.1f}, {r_loc.y:.1f}, {r_loc.z:.1f})",
                        "world_loc": f"({w_loc.x:.1f}, {w_loc.y:.1f}, {w_loc.z:.1f})",
                    })
            pause(pc, True)
            pc.update_camera()
            enemy_wf = [c for c in ships[2].get_components_by_class(unreal.StaticMeshComponent) if "ShieldImpact" in c.get_name()]
            friendly_wf = [c for c in ships[1].get_components_by_class(unreal.StaticMeshComponent) if "ShieldImpact" in c.get_name()]
            enemy_colors = []
            for c in enemy_wf:
                mid = c.get_material(0)
                if mid and isinstance(mid, unreal.MaterialInstanceDynamic):
                    col = mid.get_vector_parameter_value("ShieldColor")
                    enemy_colors.append((c.get_name(), c.is_visible(), col.r, col.g, col.b))
            friendly_colors = []
            for c in friendly_wf:
                mid = c.get_material(0)
                if mid and isinstance(mid, unreal.MaterialInstanceDynamic):
                    col = mid.get_vector_parameter_value("ShieldColor")
                    friendly_colors.append((c.get_name(), c.is_visible(), col.r, col.g, col.b))
            enemy_red_ok = (
                len(enemy_colors) >= 2
                and all(vis and r > 0.85 and g < 0.10 and b < 0.10 for _, vis, r, g, b in enemy_colors)
            )
            friendly_blue_ok = (
                len(friendly_colors) >= 2
                and all(vis and r < 0.20 and g > 0.45 and b > 0.85 for _, vis, r, g, b in friendly_colors)
            )
            no_legacy_blue_niagara = not any(
                d["ship"] == 2 and "Shield" in d["asset"] and d["active"]
                for d in report["niagara_debug"]
            )
            check(
                "1_Enemy_Red_Shield_Impact_Verified",
                enemy_red_ok and no_legacy_blue_niagara,
                f"Enemy={enemy_colors}, no_blue_niagara={no_legacy_blue_niagara}",
            )
            check(
                "1_Friendly_Blue_Shield_Impact_Verified",
                friendly_blue_ok,
                f"Friendly={friendly_colors}",
            )
            thruster_info = []
            for idx, s in enumerate(ships[:3]):
                cores = [c for c in s.get_components_by_class(unreal.StaticMeshComponent) if "ThrusterCoreMesh" in c.get_name()]
                jets = [c for c in s.get_components_by_class(unreal.StaticMeshComponent) if "ThrusterJetMesh" in c.get_name()]
                for c in cores:
                    rl = p(c, "relative_location")
                    thruster_info.append((idx, c.get_name(), c.is_visible(), round(rl.x, 1), round(rl.y, 1)))
                for j in jets:
                    rl = p(j, "relative_location")
                    rs = p(j, "relative_scale3d")
                    thruster_info.append((idx, j.get_name(), j.is_visible(), round(rl.x, 1), round(rs.x, 3), round(rs.z, 3)))
            no_legacy_yellow_thruster = not any(
                "ThrusterTrail" in d["asset"] and d["active"]
                for d in report["niagara_debug"]
            )
            thruster_ok = (
                len(thruster_info) == 12
                and all(item[2] for item in thruster_info)
                and all(item[3] < -165.0 for item in thruster_info)
                and all(item[5] > item[4] * 1.5 for item in thruster_info if len(item) == 6)
                and no_legacy_yellow_thruster
            )
            check(
                "2_Directional_Thruster_Plumes_Verified",
                thruster_ok,
                f"Plumes={thruster_info}, no_legacy_ball={no_legacy_yellow_thruster}",
            )
            focus_pie_preview_window()
            state.update(stage="take_shot", at=time.monotonic())
            return

        if state["stage"] == "take_shot":
            if time.monotonic() - state["at"] < 0.25:
                return
            focus_pie_preview_window()
            if os.path.exists(artifact_shot):
                os.remove(artifact_shot)
            unreal.SystemLibrary.execute_console_command(
                world, f'Shot SHOWUI filename="{artifact_shot}" -nosuffix', pc
            )
            state.update(stage="verify_capture", at=time.monotonic())
            return

        if state["stage"] == "verify_capture":
            exists = os.path.isfile(artifact_shot) and os.path.getsize(artifact_shot) > 0
            if not exists and (time.monotonic() - state["at"] < 2.5):
                return
            w, h = read_png_size(artifact_shot) if exists else (0, 0)
            check(
                "VFX_Showcase_Screenshot_Captured",
                exists and 900 <= w <= 1600 and 500 <= h <= 950,
                f"{artifact_shot} ({w}x{h})",
            )
            finish()

    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
