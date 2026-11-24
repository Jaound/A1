"""Exercise the actual BaseTrial actor in PIE. Human input and cooking are separate."""
import json
import time
import traceback
from pathlib import Path

import unreal as u

editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
saved=Path(u.Paths.convert_relative_path_to_full(u.Paths.project_saved_dir()))
output=saved/"Validation"/"BaseTrial3"
output.mkdir(parents=True,exist_ok=True)
report={"checks":[]}
slot=saved/"BaseTrial"/"BaseTrial.ovbase"
protected={path:(path.read_bytes() if path.is_file() else None)
           for path in (slot,slot.with_suffix(".ovbase.bak"),slot.with_suffix(".ovbase.tmp"))}
deadline=time.monotonic()+90

def check(name,value):
    report["checks"].append({"name":name,"passed":bool(value)})
    if not value:
        raise AssertionError(name)

def ok(value):
    return value[0] if isinstance(value,tuple) else value

def run():
    world=editor.get_game_world()
    while not world or not u.GameplayStatics.get_player_controller(world,0):
        if time.monotonic()>deadline:
            raise RuntimeError("PIE start timed out")
        yield
        world=editor.get_game_world()
    pc=u.GameplayStatics.get_player_controller(world,0)
    hud=pc.get_hud()
    ship=lambda:pc.get_editor_property("ships")[0]
    base=lambda:u.GameplayStatics.get_all_actors_of_class(world,u.OVBasePresentation)[0]

    def reserve(name,x,y,z=0,turn=0):
        return base().reserve_module(name,u.IntVector(x,y,z),turn)

    def tick_game(seconds):
        pc.action("Speed1")
        if pc.get_editor_property("simulation_paused"):
            pc.action("Pause")
        pc.step_simulation(seconds)
        if not pc.get_editor_property("simulation_paused"):
            pc.action("Pause")

    check("start BaseTrial",pc.start_play_scenario("BaseTrial"))
    ship().set_actor_location(u.Vector(35000,35000,10000),False,True)
    ship().set_destination(u.Vector(35000,35000,10000))
    base().set_trial_accounting(True,True)
    check("19-entry definition recognized at runtime",not reserve("NoSuchModule",3,0))
    check("first workshop starts at Core I",base().module_level(0)==1)
    check("cannot upgrade without Engineering II",not base().upgrade_module(0))
    check("trial Engineering II",base().prepare_trial_technology(1))
    check("charge Core II once",base().upgrade_module(0) and base().common_stock()==4000)
    tick_game(300)
    check("Core II completes",base().module_level(0)==2)
    check("power tier and technology gates independent",not reserve("YardMedium",0,10))
    check("prepare first extension technology",base().prepare_trial_technology(7))
    check("construction chain held",reserve("Corridor",3,0) and reserve("Storage",4,0))
    check("reserved dependency protected",not base().cancel_construction(1))
    tick_game(75)
    check("Warehouse I connected",base().is_module_connected(2) and base().advanced_capacity()==2500)
    base().set_trial_accounting(False,False)
    check("Warehouse II reservation",base().upgrade_module(2) and base().module_level(2)==1)
    check("Core III reservation",base().prepare_trial_technology(2) and base().upgrade_module(0))
    tick_game(600)
    check("individual Warehouse II completion",base().module_level(2)==2 and base().module_level(0)==3)
    check("warehouse III technology",base().prepare_trial_technology(8) and base().upgrade_module(2))
    tick_game(120)
    check("single Warehouse III supplies 5500 advanced",base().advanced_capacity()==5500)
    check("Core IV held by storage",base().prepare_trial_technology(3) and not base().upgrade_module(0))
    check("second store",reserve("Corridor",-3,0,0,2) and reserve("Storage",-4,0,0,2))
    tick_game(75)
    check("second storage is tier I",base().module_level(4)==1 and base().advanced_capacity()==7000)
    check("Core IV starts with enlarged storage",base().upgrade_module(0))
    tick_game(960)
    check("Core IV completes",base().module_level(0)==4)

    check("new scene for ring",pc.start_play_scenario("BaseTrial"))
    ship().set_actor_location(u.Vector(35000,35000,10000),False,True)
    ship().set_destination(u.Vector(35000,35000,10000))
    base().set_trial_accounting(True,True)
    for x,y in [(3,0),(4,0),(5,0),(3,1),(4,1),(5,1)]:
        check(f"build hub at {x}/{y}",reserve("Junction",x,y))
    check("build ring warehouse",reserve("Storage",6,0))
    tick_game(120)
    check("storage online",base().is_module_connected(7))
    before=base().common_stock()
    check("demolition pays 75 percent once",base().demolish_module(2) and base().common_stock()==before+30)
    check("ring preserves path",base().is_module_connected(6))
    check("last path disconnects",base().demolish_module(3) and not base().is_module_connected(5))
    check("reconnect with new hub",reserve("Junction",4,0))
    tick_game(4)
    before=base().common_stock()
    check("save pending construction",ok(base().save_trial()))
    check("restart is separate",pc.start_play_scenario("BaseTrial") and base().module_count()==1)
    ship().set_actor_location(u.Vector(35000,35000,10000),False,True)
    ship().set_destination(u.Vector(35000,35000,10000))
    check("restore pending work",ok(base().load_trial()) and base().common_stock()==before and abs(base().remaining_seconds(6)-11)<.2)
    tick_game(12)
    check("restored construction reconnects",base().is_module_connected(5) and base().common_stock()==before)
    payload=slot.read_bytes()
    slot.write_bytes(payload[:4])
    check("truncated data keeps live session",not ok(base().load_trial()) and base().is_module_connected(5))
    slot.write_bytes(payload)
    camera=pc.get_view_target()
    hud.execute_ui_action("BaseBack")
    hud.execute_ui_action("BaseView")
    check("shared camera and base survive toggling",pc.get_view_target()==camera and base().is_module_connected(5))

def tick(_):
    try:
        next(job)
        return
    except StopIteration:
        report["passed"]=True
    except Exception:
        report["passed"]=False
        report["error"]=traceback.format_exc()
    finally:
        if "passed" in report:
            for path,content in protected.items():
                if content is not None:
                    path.write_bytes(content)
                elif path.exists():
                    path.unlink()
            (output/"PIE.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
            u.unregister_slate_post_tick_callback(handle)
            u.SystemLibrary.quit_editor()

job=run()
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
handle=u.register_slate_post_tick_callback(tick)
