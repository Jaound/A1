"""원본 선체의 상·하·좌·우 정사영을 무장 배치 참고용으로 출력한다.

Blender 백그라운드 전용. Prepared FBX를 읽기만 하며 기존 출력은 덮어쓰지 않는다.
최종 배치·피격·포탑 가동 검증이나 정밀 좌표 측정 결과는 아니다.
"""
from pathlib import Path
import json
import math
import bpy
from mathutils import Vector

root=Path.home()/'Documents/OrbitalVanguardAssets'
output=root/'WeaponPlacementGuide_v1'/'views'
output.mkdir(parents=True,exist_ok=False)
sources={'corvette':root/'Prepared/PlayUI_R2_PC14_FullMesh_v1/OV_PC14_FullMesh.fbx'}
for tag in ('PlayUI_R2_DestroyerFull_v1','PlayUI_R2_CarrierAssaultFull_v1',
            'PlayUI_R2_MissileDefenseFull_v1','PlayUI_R2_FrigateCruiserRadarFull_v1',
            'PlayUI_R2_SupportShipsFull_v1','PlayUI_R2_HeavyShipsFull_v1'):
    directory=root/'Prepared'/tag
    for item in json.loads((directory/'report.json').read_text(encoding='utf8')):
        ident=item['id'];sources[ident]=directory/ident/('OV_'+ident+'_FullMesh.fbx')
if len(sources)!=16:raise RuntimeError('함체 수가 16개가 아닙니다')
results=[]
for ident,source in sources.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source),use_image_search=False)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    if not meshes:raise RuntimeError(ident+': 메시 없음')
    points=[o.matrix_world @ Vector(corner) for o in meshes for corner in o.bound_box]
    low=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high=Vector(tuple(max(p[i] for p in points) for i in range(3)))
    center=(low+high)/2;ext=high-low
    if ext.x<=max(ext.y,ext.z):raise RuntimeError(ident+': X 장축 확인 필요')
    scene=bpy.context.scene;scene.render.engine='BLENDER_WORKBENCH'
    scene.display.shading.light='STUDIO';scene.display.shading.color_type='SINGLE'
    scene.display.shading.single_color=(.62,.68,.73);scene.display.shading.show_cavity=True
    scene.display.shading.background_type='WORLD'
    if not scene.world:scene.world=bpy.data.worlds.new('GuideWorld')
    scene.world.color=(.025,.035,.05)
    scene.render.resolution_x=900;scene.render.resolution_y=500;scene.render.resolution_percentage=100
    scene.render.image_settings.file_format='PNG'
    camera=bpy.data.cameras.new('GuideCamera');cam=bpy.data.objects.new('GuideCamera',camera)
    scene.collection.objects.link(cam);scene.camera=cam;camera.type='ORTHO';camera.ortho_scale=max(ext.x*1.28,ext.y*2,ext.z*2)
    target=output/ident;target.mkdir()
    # Source bow is -X. Projection is flipped later when necessary so bow is always right.
    for name,direction in [('top',Vector((0,0,1))),('bottom',Vector((0,0,-1))),
                           ('left',Vector((0,1,0))),('right',Vector((0,-1,0)))]:
        cam.location=center+direction*max(ext)*3
        cam.rotation_euler=(-direction).to_track_quat('-Z','Y').to_euler()
        right=cam.rotation_euler.to_matrix()@Vector((1,0,0))
        path=target/(name+'.png');scene.render.filepath=str(path);bpy.ops.render.render(write_still=True)
        results.append({'id':ident,'view':name,'file':str(path),'flip_x':right.x>0})
    (output/'report.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf8')
    print('OV_PLACEMENT_VIEW',ident,flush=True)
