"""10초 실제 PIE 렌더를 연속 PNG와 시각/조준 기록으로 저장한다. 직접 입력 검사는 아니다.

OV_WEAPON_VIDEO_SECONDS(기본10), OV_WEAPON_VIDEO_FPS(기본30), OV_WEAPON_VIDEO_SPEED(1/2/3).
실제 벽시각 간격으로 캡처하며 프레임 누락/지연을 JSON에 기록한다. PNG 읽기/압축 부하가
포함되므로 성능 벤치마크가 아니다. ffmpeg가 있으면 실제 캡처 간격을 사용해 MP4를 만든다.
"""
import glob
import json
import os
import shutil
import subprocess
import time
import traceback
import unreal

saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
stamp = time.strftime("%Y%m%d_%H%M%S") + "_%06d" % (time.time_ns() % 1000000)
out_dir = os.path.join(saved, "Validation", "WeaponVideo", stamp)
os.makedirs(out_dir, exist_ok=False)
seconds = max(4, min(30, float(os.environ.get("OV_WEAPON_VIDEO_SECONDS", "10"))))
fps = max(5, min(30, int(os.environ.get("OV_WEAPON_VIDEO_FPS", "30"))))
speed = max(1, min(3, int(os.environ.get("OV_WEAPON_VIDEO_SPEED", "1"))))
report = {"passed": None, "kind": "실제 PIE 프레임 캡처", "physical_input": False,
          "scope": "가동 임시 포탑 실제 렌더. API로 표적/이동/자동 사격 설정; 직접 입력·성능 판정 아님",
          "scene": "F5 transient turret presentation fixture", "speed": speed, "seed": 260112,
          "requested_seconds": seconds, "requested_fps": fps, "frames": [], "errors": []}
project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
identity = os.path.join(project, "Config", "BuildIdentity.generated.json")
if os.path.isfile(identity):
    with open(identity, encoding="utf-8-sig") as stream:
        report["build_identity"] = json.load(stream)
state = {"stage": "start", "started": time.monotonic()}
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)


def p(obj, key):
    return obj.get_editor_property(key)


def vector(v):
    return [v.x, v.y, v.z]


def snapshot(ship):
    return [{"mount": str(p(v, "mount_id")), "logical_aim": [p(v, "logical_aim").pitch, p(v, "logical_aim").yaw],
             "visual_aim": [p(v, "visual_aim").pitch, p(v, "visual_aim").yaw],
             "world_muzzle_cm": vector(p(v, "world_muzzle")), "flash": p(v, "flash_visible"),
             "shot_id": p(v, "last_shot_id"), "shot_game_time": p(v, "last_shot_time"),
             "vls": p(v, "fixed_vls"), "destroyed": p(v, "destroyed")}
            for v in ship.get_weapon_visual_states()]


def encode():
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        matches = glob.glob(os.path.join(os.environ.get("LOCALAPPDATA", ""), "Microsoft", "WinGet", "Packages", "Gyan.FFmpeg*", "*", "bin", "ffmpeg.exe"))
        ffmpeg = matches[0] if matches else None
    report["ffmpeg"] = ffmpeg
    frames = report["frames"]
    if not ffmpeg or len(frames) < 2:
        report["video_status"] = "ffmpeg 또는 충분한 프레임 없음; PNG/시각 기록만 저장"
        return
    # Concat durations preserve the real intervals rather than pretending delayed capture was 30 fps.
    listing = os.path.join(out_dir, "frames.ffconcat")
    with open(listing, "w", encoding="utf-8") as stream:
        stream.write("ffconcat version 1.0\n")
        for index, frame in enumerate(frames):
            duration = (frames[index + 1]["wall_seconds"] - frame["wall_seconds"]) if index + 1 < len(frames) else 1 / fps
            stream.write("file '%s'\nduration %.9f\n" % (os.path.basename(frame["path"]), max(.001, duration)))
        stream.write("file '%s'\n" % os.path.basename(frames[-1]["path"]))
    path = os.path.join(out_dir, "포탑_실제렌더.mp4")
    process = subprocess.run([ffmpeg, "-y", "-hide_banner", "-loglevel", "error", "-f", "concat", "-safe", "0", "-i", listing,
                              "-vf", "pad=ceil(iw/2)*2:ceil(ih/2)*2", "-fps_mode", "vfr", "-c:v", "libx264", "-preset", "fast",
                              "-crf", "20", "-pix_fmt", "yuv420p", path], capture_output=True, text=True, timeout=45,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    report["video_status"] = "완료" if process.returncode == 0 and os.path.isfile(path) else "인코딩 실패"
    report["video_path"] = path
    report["encoder_error"] = process.stderr[-2000:]


def finish():
    try:
        if state.get("floating"):
            report["play_settings_restored"] = bool(unreal.OVDevelopmentEditorLibrary.restore_preview_settings())
        missing = [f["path"] for f in report["frames"] if not os.path.isfile(f["path"])]
        report["missing_frames"] = missing
        if missing:
            report["errors"].append("누락 PNG %d장" % len(missing))
        elif report["frames"]:
            encode()
        frames = report["frames"]
        report["recorded_seconds"] = frames[-1]["wall_seconds"] - frames[0]["wall_seconds"] if len(frames) > 1 else 0
        report["effective_capture_fps"] = (len(frames) - 1) / report["recorded_seconds"] if report["recorded_seconds"] else 0
        report["passed"] = len(frames) > 10 and not report["errors"]
    except Exception:
        report["errors"].append(traceback.format_exc())
        report["passed"] = False
    with open(os.path.join(out_dir, "report.json"), "w", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    unreal.log("OV 포탑 영상: " + out_dir)
    unreal.unregister_slate_post_tick_callback(state["handle"])


def tick(_dt):
    try:
        if time.monotonic() - state["started"] > 100:
            raise RuntimeError("포탑 캡처 시간 초과")
        if state["stage"] == "start":
            if not unreal.OVDevelopmentEditorLibrary.start_preview_window(1920, 1080):
                raise RuntimeError("별도 PIE 창 시작 실패")
            state.update(stage="wait", floating=True)
            return
        world = editor.get_game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if not pc or len(p(pc, "ships")) < 3:
            return
        if state["stage"] == "wait":
            pc.start_combat_range()
            ships = list(p(pc, "ships"))
            source, target = ships[1], ships[2]
            for ship in ships:
                ship.set_automation(False, False)
                ship.stop_movement()
            source.set_actor_location(unreal.Vector(0, 0, 0), False, False)
            target.set_actor_location(unreal.Vector(3500, 2500, 650), False, False)
            ships[0].set_actor_location(unreal.Vector(-15000, 10000, 0), False, False)
            # Explicit visual fixture speed, never persisted to production hull/profile data.
            target.set_editor_property("move_speed", 650)
            target.set_editor_property("turn_rate", 90)
            target.set_destination(unreal.Vector(3500, -2500, 650))
            p(pc, "combat").set_target(source, target)
            source.set_automation(False, True)
            pc.select_ship(source)
            pc.set_editor_property("camera_focus", unreal.Vector(0, 0, 0))
            pc.set_editor_property("camera_distance", 2050)
            pc.set_editor_property("camera_yaw", -50)
            pc.set_editor_property("camera_pitch", -32)
            pc.update_camera()
            if p(pc, "developer_mode"):
                pc.action("Developer")
            pc.get_hud().set_details_open(True)
            pc.action("Speed%d" % speed)
            report["viewport_pixels"] = list(pc.get_viewport_size())
            report["fixture_only"] = {"target_move_speed_cm_s": 650, "target_turn_deg_s": 90,
                                    "target_start_cm": [3500, 2500, 650], "target_destination_cm": [3500, -2500, 650]}
            state.update(stage="warmup", at=time.monotonic(), source=source, target=target)
            return
        if state["stage"] == "warmup":
            if time.monotonic() - state["at"] < 1:
                return
            if p(pc, "simulation_paused"):
                pc.action("Pause")
            state.update(stage="record", at=time.monotonic(), last=-100, frame=0)
            return
        if state["stage"] == "record":
            elapsed = time.monotonic() - state["at"]
            if elapsed >= seconds:
                if not p(pc, "simulation_paused"):
                    pc.action("Pause")
                state.update(stage="drain", at=time.monotonic())
                return
            if elapsed - state["last"] < 1 / fps:
                return
            state["last"] = elapsed
            path = os.path.abspath(os.path.join(out_dir, "frame_%04d.png" % state["frame"])).replace("\\", "/")
            combat = p(pc, "combat")
            report["frames"].append({"path": path, "wall_seconds": elapsed, "game_seconds": p(combat, "game_time"),
                                     "shots": p(combat, "shot_count"), "hits": p(combat, "hit_count"),
                                     "target_cm": vector(state["target"].get_actor_location()), "mounts": snapshot(state["source"])})
            unreal.SystemLibrary.execute_console_command(world, 'Shot SHOWUI filename="' + path + '" -nosuffix', pc)
            state["frame"] += 1
        elif state["stage"] == "drain" and time.monotonic() - state["at"] > 1:
            finish()
    except Exception:
        report["errors"].append(traceback.format_exc())
        unreal.log_error(report["errors"][-1])
        finish()


state["handle"] = unreal.register_slate_post_tick_callback(tick)
