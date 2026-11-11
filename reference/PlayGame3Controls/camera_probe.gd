extends SceneTree

# Run against the original extracted project, not as a standalone game.
var checks: Array[Dictionary] = []

func record(name: String, ok: bool, details: Variant = null) -> void:
	checks.append({"name": name, "passed": ok, "details": details})

func key(code: Key, pressed: bool) -> void:
	var ev := InputEventKey.new()
	ev.keycode = code
	ev.physical_keycode = code
	ev.pressed = pressed
	Input.parse_input_event(ev)
	Input.flush_buffered_events()

func mouse_button(button: MouseButton, pressed: bool, alt: bool = false) -> InputEventMouseButton:
	var ev := InputEventMouseButton.new()
	ev.button_index = button
	ev.pressed = pressed
	ev.alt_pressed = alt
	return ev

func motion(x: float, y: float) -> InputEventMouseMotion:
	var ev := InputEventMouseMotion.new()
	ev.relative = Vector2(x, y)
	return ev

func _initialize() -> void:
	call_deferred("run_probe")

func run_probe() -> void:
	var manager := root.get_node("GameManager")
	manager.set_process(false)
	manager.clear_selection()
	var viewport := SubViewport.new()
	viewport.size = Vector2i(1600, 900)
	viewport.own_world_3d = true
	root.add_child(viewport)
	var rig = load("res://src/view3d/RtsCameraRig.gd").new()
	viewport.add_child(rig)
	rig.set_process(false)
	rig.camera.current = true
	await process_frame

	var initial_pivot: Vector3 = rig.pivot_pos
	var initial_yaw: float = rig.yaw_deg
	var initial_pitch: float = rig.pitch_deg
	var initial_distance: float = rig.distance
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_MIDDLE, true))
	rig.handle_viewport_input(motion(100.0, 50.0))
	record("MMB orbit angular response", is_equal_approx(rig.yaw_deg, initial_yaw - 32.0) and is_equal_approx(rig.pitch_deg, initial_pitch + 14.0))
	record("Orbit preserves pivot and distance", rig.pivot_pos.is_equal_approx(initial_pivot) and is_equal_approx(rig.distance, initial_distance))
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_MIDDLE, false))
	var after_release: float = rig.yaw_deg
	rig.handle_viewport_input(motion(100.0, 0.0))
	record("MMB release stops orbit", is_equal_approx(rig.yaw_deg, after_release))
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_LEFT, true, true))
	rig.handle_viewport_input(motion(10.0, 0.0))
	record("Alt LMB orbit", is_equal_approx(rig.yaw_deg, after_release - 3.2))
	rig.handle_viewport_input(motion(0.0, 10000.0))
	record("Pitch upper clamp", is_equal_approx(rig.pitch_deg, 84.0))
	rig.handle_viewport_input(motion(0.0, -10000.0))
	record("Pitch lower clamp", is_equal_approx(rig.pitch_deg, 12.0))
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_LEFT, false, true))
	rig.distance = 290.0
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_WHEEL_UP, true))
	record("Proportional wheel in", is_equal_approx(rig.distance, 255.2))
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_WHEEL_DOWN, true))
	record("Proportional wheel out", is_equal_approx(rig.distance, 288.376), {"in_out_multiplier": 0.88 * 1.13})

	rig.distance = 290.0
	rig.yaw_deg = -32.0
	rig.pitch_deg = 38.0
	rig.pivot_pos = Vector3.ZERO
	key(KEY_W, true)
	rig._process(0.1)
	key(KEY_W, false)
	record("W pan at zoom 290", is_equal_approx(rig.pivot_pos.length(), 21.75) and is_zero_approx(rig.pivot_pos.y), {"distance": rig.pivot_pos.length()})
	rig.pivot_pos = Vector3.ZERO
	key(KEY_W, true)
	key(KEY_D, true)
	rig._process(0.1)
	key(KEY_W, false)
	key(KEY_D, false)
	record("Diagonal pan normalized", is_equal_approx(rig.pivot_pos.length(), 21.75))
	var before_q: float = rig.yaw_deg
	key(KEY_Q, true)
	rig._process(0.1)
	key(KEY_Q, false)
	record("Q continuous yaw 75 deg per second", is_equal_approx(rig.yaw_deg, before_q + 7.5))
	manager.is_paused = true
	var paused_pivot: Vector3 = rig.pivot_pos
	key(KEY_W, true)
	rig._process(0.1)
	key(KEY_W, false)
	record("Camera pan independent of simulation pause flag", rig.pivot_pos.distance_to(paused_pivot) > 1.0)

	rig.distance = 290.0
	rig.yaw_deg = -32.0
	rig.pitch_deg = 38.0
	rig._update_camera_transform()
	var camera: Camera3D = rig.camera
	var center: Vector3 = camera.project_ray_normal(Vector2(800, 450))
	var edge_top: Vector3 = camera.project_ray_normal(Vector2(800, 0))
	var edge_right: Vector3 = camera.project_ray_normal(Vector2(1600, 450))
	var vertical_fov: float = rad_to_deg(center.angle_to(edge_top)) * 2.0
	var horizontal_fov: float = rad_to_deg(center.angle_to(edge_right)) * 2.0
	# Negative edge case: Alt is released before the left button.
	rig.is_orbiting = false
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_LEFT, true, true))
	rig.handle_viewport_input(mouse_button(MOUSE_BUTTON_LEFT, false, false))
	var yaw_before_edge: float = rig.yaw_deg
	rig.handle_viewport_input(motion(10.0, 0.0))
	var edge_observations := [{
		"name": "Alt released before LMB release",
		"orbit_remains_active": rig.is_orbiting,
		"motion_still_rotates_camera": not is_equal_approx(rig.yaw_deg, yaw_before_edge),
		"classification": "Observed source edge bug; UE port should release by captured gesture button, not modifier state"
	}]
	rig.is_orbiting = false
	var failed: int = checks.filter(func(c): return not c.passed).size()
	var result := {
		"engine": Engine.get_version_info().string,
		"method": "Headless source camera event/API probe; no OS mouse input, no UE runtime validation",
		"passed": checks.size() - failed,
		"failed": failed,
		"checks": checks,
		"edge_observations": edge_observations,
		"projection": {"viewport": "1600x900", "configured_fov": camera.fov, "keep_aspect": camera.keep_aspect, "vertical_fov": vertical_fov, "horizontal_fov": horizontal_fov}
	}
	print("CAMERA_PROBE_JSON=" + JSON.stringify(result))
	quit(0 if failed == 0 else 1)
