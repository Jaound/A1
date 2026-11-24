"""200코스트 편성에서 항모 간격별 공중전을 같은 초기 조건으로 반복한다."""
from collections import Counter
from statistics import median
from pathlib import Path
import json
import traceback
import unreal as u

editor = u.get_editor_subsystem(u.UnrealEditorSubsystem)
tag = "before" if "-OVWingTag=before" in u.SystemLibrary.get_command_line() else "after"
out = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir())) / "Validation"
out.mkdir(exist_ok=True)
result = {"runs": []}

def run():
    while not editor.get_game_world():
        yield
    world = editor.get_game_world()
    pc = u.GameplayStatics.get_player_controller(world, 0)
    pc.get_hud().close_title_screen()
    for gap in (200, 220, 240):
        assert pc.start_play_scenario("Fleet200")
        wing = pc.get_editor_property("carrier")
        carriers = [c.carrier for c in wing.carriers]
        a = next(s for s in carriers if s.friendly)
        b = next(s for s in carriers if not s.friendly)
        # 양쪽 모함을 동일한 별도 공간에 놓고 함재기 시계만 진행한다.
        a.set_actor_location(u.Vector(0,600000,0),False,False)
        b.set_actor_location(u.Vector(gap*100,600000,0),False,False)
        pc.select_ship(a)
        pc.action("WingAirCombat")
        for ship in (a,b):
            assert wing.launch(ship)
        first_loss = None
        peak_focus = 0
        seen_targets = set()
        first_lock = {}
        destroyed_at = {}
        for half in range(1,81):
            for _ in range(10):
                wing.simulate(.05)
            crafts = list(wing.aircraft)
            locks = Counter(c.intercept_target_id for c in crafts if c.carrier==a and c.state==u.OVAircraftState.SORTIE and c.intercept_target_id)
            seen_targets.update(locks)
            for target_id in locks:
                first_lock.setdefault(target_id, half/2)
            for c in crafts:
                if c.carrier==b and c.state==u.OVAircraftState.WRECKED:
                    destroyed_at.setdefault(c.aircraft_id, half/2)
            peak_focus = max(peak_focus, max(locks.values(), default=0))
            losses = sum(c.carrier==b and c.state==u.OVAircraftState.WRECKED for c in crafts)
            if losses and first_loss is None:
                first_loss = half/2
        elapsed = [t-first_lock[i] for i,t in destroyed_at.items() if i in first_lock]
        friendly_losses = sum(c.carrier==a and c.state==u.OVAircraftState.WRECKED for c in crafts)
        result["runs"].append(dict(gap_m=gap, first_enemy_loss_s=first_loss, enemy_losses_at_40s=losses,
                                    friendly_losses_at_40s=friendly_losses, timed_losses=len(elapsed),
                                    median_lock_to_loss_s=median(elapsed) if elapsed else None,
                                    max_sampled_pursuers=peak_focus, distinct_targets=len(seen_targets)))
        yield
    if tag == "after":
        for row in result["runs"]:
            assert 0 < row["max_sampled_pursuers"] <= 3 and row["enemy_losses_at_40s"] >= 10, row
    result["passed"] = True

def tick(_dt):
    try:
        next(job)
        return
    except StopIteration:
        pass
    except Exception:
        result["error"] = traceback.format_exc()
    (out / ("FighterScreen_"+tag+".json")).write_text(json.dumps(result, indent=2), encoding="utf-8")
    u.unregister_slate_post_tick_callback(handle)
    u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
