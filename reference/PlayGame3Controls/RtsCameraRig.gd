class_name RtsCameraRig
extends Node3D

## RTS 3D 카메라 (WASD/방향키 이동, Q/E 또는 Alt/휠클릭 회전, 휠 줌, 일시정지 중에도 작동)

@export var pan_speed: float = 180.0
@export var zoom_min: float = 35.0
@export var zoom_max: float = 680.0

var pivot_pos: Vector3 = Vector3.ZERO
var yaw_deg: float = -32.0
var pitch_deg: float = 38.0
var distance: float = 290.0
var is_orbiting: bool = false
var is_panning_mmb: bool = false
var camera: Camera3D = null

func _ready() -> void:
	camera = Camera3D.new()
	camera.name = "RTSCamera"
	camera.far = 4500.0
	camera.near = 0.5
	camera.fov = 52.0
	add_child(camera)
	_update_camera_transform()
	GameManager.camera_focus_requested.connect(_on_focus_requested)

func _on_focus_requested(world_pos: Vector3) -> void:
	pivot_pos = world_pos
	_update_camera_transform()

func get_effective_zoom_min() -> float:
	var p := GameManager.get_primary_selected_ship()
	if p != null and not p.is_fighter:
		return maxf(zoom_min, p.half_extents.length() * 1.45)
	return zoom_min

func handle_viewport_input(event: InputEvent) -> bool:
	var eff_min := get_effective_zoom_min()
	if event is InputEventMouseButton:
		var mb := event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_WHEEL_UP and mb.pressed:
			distance = clampf(distance * 0.88, eff_min, zoom_max)
			_update_camera_transform()
			return true
		elif mb.button_index == MOUSE_BUTTON_WHEEL_DOWN and mb.pressed:
			distance = clampf(distance * 1.13, eff_min, zoom_max)
			_update_camera_transform()
			return true
		elif mb.button_index == MOUSE_BUTTON_MIDDLE:
			if mb.alt_pressed:
				is_orbiting = mb.pressed
				is_panning_mmb = false
			else:
				is_orbiting = mb.pressed # 휠 드래그로 3D 각도 회전
			return true
		elif mb.button_index == MOUSE_BUTTON_LEFT and mb.alt_pressed:
			is_orbiting = mb.pressed
			return true
	elif event is InputEventMouseMotion:
		var mm := event as InputEventMouseMotion
		if is_orbiting:
			yaw_deg -= mm.relative.x * 0.32
			pitch_deg = clampf(pitch_deg + mm.relative.y * 0.28, 12.0, 84.0)
			_update_camera_transform()
			return true
	return false

func _process(delta: float) -> void:
	# 일시정지 중에도 카메라는 독립적으로 부드럽게 작동
	var move_dir := Vector3.ZERO
	var yaw_rad := deg_to_rad(yaw_deg)
	var cam_fwd := Vector3(-sin(yaw_rad), 0.0, -cos(yaw_rad))
	var cam_right := Vector3(cos(yaw_rad), 0.0, -sin(yaw_rad))

	if Input.is_key_pressed(KEY_W) or Input.is_key_pressed(KEY_UP):
		move_dir += cam_fwd
	if Input.is_key_pressed(KEY_S) or Input.is_key_pressed(KEY_DOWN):
		move_dir -= cam_fwd
	if Input.is_key_pressed(KEY_A) or Input.is_key_pressed(KEY_LEFT):
		move_dir -= cam_right
	if Input.is_key_pressed(KEY_D) or Input.is_key_pressed(KEY_RIGHT):
		move_dir += cam_right
	if Input.is_key_pressed(KEY_Q):
		yaw_deg += 75.0 * delta
	if Input.is_key_pressed(KEY_E):
		yaw_deg -= 75.0 * delta
	if Input.is_key_pressed(KEY_R):
		var p := GameManager.get_primary_selected_ship()
		if p == null or p.ship_kind != Enums.ShipKind.SUPPLY_REPAIR:
			pivot_pos.y += pan_speed * 0.5 * delta
	if Input.is_key_pressed(KEY_C):
		pivot_pos.y -= pan_speed * 0.5 * delta

	if move_dir.length_squared() > 0.001:
		GameManager.camera_follow_ship_id = ""
		var zoom_scale := clampf(distance / 240.0, 0.45, 2.5)
		pivot_pos += move_dir.normalized() * pan_speed * zoom_scale * delta
	elif not GameManager.camera_follow_ship_id.is_empty():
		var sec := GameManager.get_active_sector()
		var follow_ship := sec.get_ship(GameManager.camera_follow_ship_id) if sec != null else null
		if follow_ship != null and follow_ship.alive and not follow_ship.docked_in_hangar:
			pivot_pos = pivot_pos.lerp(follow_ship.position, minf(1.0, delta * 10.0))
		else:
			GameManager.camera_follow_ship_id = ""

	distance = clampf(distance, get_effective_zoom_min(), zoom_max)
	_update_camera_transform()

func _update_camera_transform() -> void:
	if camera == null:
		return
	var yaw_r := deg_to_rad(yaw_deg)
	var pitch_r := deg_to_rad(pitch_deg)
	var offset := Vector3(
		sin(yaw_r) * cos(pitch_r),
		sin(pitch_r),
		cos(yaw_r) * cos(pitch_r)
	) * distance
	camera.global_position = pivot_pos + offset
	camera.look_at(pivot_pos, Vector3.UP)
