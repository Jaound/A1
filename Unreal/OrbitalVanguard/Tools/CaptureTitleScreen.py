import unreal
import time
import os
import shutil

print(">>> CAPTURE TITLE SCREEN SCRIPT START <<<")

level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

artifact_dir = r"C:\Users\user\.gemini\antigravity\brain\0da5275e-6784-4e30-ab80-f739659cc0ff"
saved_screenshots = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Screenshots")

state = {
    "phase": 0,
    "start": time.monotonic(),
    "wait_until": 0
}

def on_tick(delta_seconds):
    try:
        now = time.monotonic()
        if now - state["start"] > 60:
            unreal.log_error("CaptureTitleScreen timeout!")
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.execute_console_command(None, "Quit")
            return

        # Phase 0: Request PIE
        if state["phase"] == 0:
            level.editor_request_begin_play()
            state["phase"] = 1
            state["wait_until"] = now + 2.0
            return

        # Phase 1: Wait for world & controller
        if state["phase"] == 1:
            if now < state["wait_until"]:
                return
            world = editor.get_game_world()
            if not world:
                return
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            if not pc:
                return
            hud = pc.get_hud()
            if not hud:
                return

            state["world"] = world
            state["pc"] = pc
            state["hud"] = hud

            # Ensure Title Screen is open
            hud.open_title_screen()
            title_widget = hud.get_title_screen()
            state["title_widget"] = title_widget

            unreal.log("OV CAPTURE: Title Screen Opened! Waiting for render to settle...")
            state["phase"] = 2
            state["wait_until"] = now + 2.5
            return

        # Phase 2: Capture Screen 01 (Title Screen)
        if state["phase"] == 2:
            if now < state["wait_until"]:
                return

            world = state["world"]
            unreal.log("OV CAPTURE: Taking Screen 01 HighResShot...")
            shot_path = os.path.join(artifact_dir, "01_TitleScreen_Render.png")
            unreal.SystemLibrary.execute_console_command(world, f"HighResShot 1280x720 filename=\"{shot_path}\"")

            # Switch to Screen 02 (Scenario Select)
            title_widget = state["title_widget"]
            title_widget.show_panel(unreal.OVTitleMenuPanel.SCENARIO_SELECT)
            title_widget.select_scenario("Fleet200")

            unreal.log("OV CAPTURE: Switched to Screen 02 (Scenario Select)! Waiting...")
            state["phase"] = 3
            state["wait_until"] = now + 2.0
            return

        # Phase 3: Capture Screen 02 (Scenario Select)
        if state["phase"] == 3:
            if now < state["wait_until"]:
                return

            world = state["world"]
            unreal.log("OV CAPTURE: Taking Screen 02 HighResShot...")
            shot_path = os.path.join(artifact_dir, "02_MissionSelect_Render.png")
            unreal.SystemLibrary.execute_console_command(world, f"HighResShot 1280x720 filename=\"{shot_path}\"")

            state["phase"] = 4
            state["wait_until"] = now + 1.5
            return

        # Phase 4: Complete and Quit
        if state["phase"] == 4:
            if now < state["wait_until"]:
                return
            unreal.log("OV CAPTURE: Done! Exiting...")
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.SystemLibrary.execute_console_command(None, "Quit")

    except Exception as e:
        unreal.log_error(f"OV CAPTURE EXCEPTION: {e}")
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.execute_console_command(None, "Quit")

handle = unreal.register_slate_post_tick_callback(on_tick)
