"""Create the tiny, portable control-range map using only built-in engine assets.
Run with UnrealEditor-Cmd Project.uproject -run=pythonscript -script=... -unattended.
Does not overwrite an existing ControlRange map.
"""
import unreal
path = '/Game/Maps/ControlRange'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.log('OV: Existing ControlRange map preserved.')
else:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.new_level(path):
        raise RuntimeError('Could not create ControlRange map')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,2000), unreal.Rotator(-50,-25,0))
    sun.set_actor_label('Range key light')
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property('forward_shading_priority',1)
    sun.light_component.set_intensity(3.0)
    fill = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0,0,1500), unreal.Rotator(-35,150,0))
    fill.set_actor_label('Range fill light')
    fill.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    fill.light_component.set_intensity(1.5)
    levels.set_level_viewport_camera_info(unreal.Vector(-3000,-3000,5000),unreal.Rotator(-45,45,0), "")
    if not levels.save_current_level():
        raise RuntimeError('ControlRange save failed')
    unreal.log('OV: Saved /Game/Maps/ControlRange')

