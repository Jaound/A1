"""반입용 FBX의 동일한 사시도에서 건설 메뉴 카드 이미지를 렌더한다. Blender -b -P로 실행."""
from pathlib import Path
import bpy
from mathutils import Vector

root=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/BaseTrial2'
for module in ('YardSmall','DockStandard'):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root/module/('SM_OV_Base_'+module+'.fbx')))
    mesh=next(o for o in bpy.context.scene.objects if o.type=='MESH')
    corners=[mesh.matrix_world @ Vector(v) for v in mesh.bound_box]
    center=sum(corners,Vector())/8
    scene=bpy.context.scene
    bpy.ops.object.camera_add(location=center+Vector((-140,-210,135)))
    camera=bpy.context.object
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.type='ORTHO'
    camera.data.ortho_scale=155
    camera.data.clip_end=1000
    scene.camera=camera
    scene.render.engine='BLENDER_WORKBENCH'
    scene.display.shading.light='STUDIO'
    scene.display.shading.color_type='SINGLE'
    scene.display.shading.single_color=(.56,.72,.82)
    scene.display.shading.show_cavity=True
    scene.display.shading.show_shadows=True
    scene.render.film_transparent=True
    scene.render.resolution_x,scene.render.resolution_y=512,256
    scene.render.resolution_percentage=100
    scene.render.filepath=str(root/('T_'+module+'_Card.png'))
    bpy.ops.render.render(write_still=True)
