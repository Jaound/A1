"""Bake original weapon geometry with Blender, or import the baked PNGs in the UE editor.

Blender: blender -b --python BakeSortieWeaponCards.py -- <prepared 9-model folder> <anti-ship module FBX> <PNG folder>
UE: set OV_SORTIE_CARD_SOURCE to the PNG folder, then run this file with the PythonScript commandlet.
"""
import sys
from pathlib import Path

MODELS = (
    "laser_turret_lb30", "plasma_heavy_cannon", "naval_gun_bf120",
    "heavy_cannon_shc400", "missile_pod_mm24", "missile_pod_mm48",
    "railgun_rg200", "ciws_beam_b", "ciws_gatling_6", "anti_ship_module",
)


def bake(prepared, anti_ship, output):
    import bpy
    from mathutils import Vector

    output.mkdir(parents=True, exist_ok=True)
    for model in MODELS:
        source = anti_ship if model == "anti_ship_module" else next((prepared / model).glob("*.fbx"))
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(source), use_image_search=False)
        scene = bpy.context.scene
        corners = [obj.matrix_world @ Vector(corner) for obj in scene.objects if obj.type == "MESH" for corner in obj.bound_box]
        if not corners:
            raise ValueError(f"No mesh in {source}")
        low = Vector(tuple(min(p[i] for p in corners) for i in range(3)))
        high = Vector(tuple(max(p[i] for p in corners) for i in range(3)))
        center = (low + high) * .5
        direction = Vector((-.82, -1.7, .85)).normalized()
        camera_data = bpy.data.cameras.new("Weapon camera")
        camera = bpy.data.objects.new("Weapon camera", camera_data)
        scene.collection.objects.link(camera)
        distance = max((high - low).length, .01)
        camera.location = center + direction * (3 * distance)
        camera.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()
        local = camera.rotation_euler.to_matrix().inverted()
        projected = [local @ (p - center) for p in corners]
        camera_data.type = "ORTHO"
        camera_data.ortho_scale = 2.1 * max(max(abs(p.x) for p in projected), max(abs(p.y) for p in projected) * 1.5)
        camera_data.clip_start = distance * .01
        camera_data.clip_end = distance * 8
        scene.camera = camera
        scene.render.engine = "BLENDER_WORKBENCH"
        shading = scene.display.shading
        shading.light = "STUDIO"
        shading.color_type = "SINGLE"
        shading.single_color = (.64, .77, .83)
        shading.show_cavity = True
        shading.cavity_type = "WORLD"
        shading.show_shadows = True
        scene.render.film_transparent = True
        scene.render.image_settings.color_mode = "RGBA"
        scene.render.resolution_x = 300
        scene.render.resolution_y = 200
        scene.render.resolution_percentage = 100
        scene.render.filepath = str(output / f"{model}.png")
        bpy.ops.render.render(write_still=True)


def import_cards(source):
    import unreal as u

    folder = "/Game/UI/MissionCards/Weapons"
    u.EditorAssetLibrary.make_directory(folder)
    for model in MODELS:
        filename = source / f"{model}.png"
        if not filename.is_file():
            raise FileNotFoundError(filename)
        task = u.AssetImportTask()
        task.filename = str(filename)
        task.destination_path = folder
        task.destination_name = model
        task.automated = True
        task.replace_existing = True
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = u.load_asset(f"{folder}/{model}")
        if not isinstance(texture, u.Texture2D):
            raise RuntimeError(f"Failed to import {model}")
        texture.set_editor_property("lod_group", u.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("compression_settings", u.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("srgb", True)
        if not u.EditorAssetLibrary.save_loaded_asset(texture):
            raise RuntimeError(f"Failed to save {model}")


if __name__ == "__main__":
    if "--" in sys.argv:
        bake(*(Path(arg) for arg in sys.argv[sys.argv.index("--")+1:]))
    else:
        import os
        import_cards(Path(os.environ["OV_SORTIE_CARD_SOURCE"]))
