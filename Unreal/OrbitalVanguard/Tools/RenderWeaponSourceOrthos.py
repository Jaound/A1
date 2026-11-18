"""원본 9종의 정면·측면 정사영을 저장한다. 원본은 변경하지 않는다."""
import hashlib
import json
from pathlib import Path
import bpy
from mathutils import Vector

root=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponFullSources_v1'
project=Path(__file__).resolve().parents[1]
out=project/'Saved/Validation/WeaponSourceOrthos'
if out.exists():raise FileExistsError(out)
out.mkdir(parents=True)
models=json.loads((root/'report.json').read_text(encoding='utf8'))['models']
rows=[]
for model in models:
    path=Path(model['prepared_fbx'])
    with path.open('rb') as stream:digest=hashlib.file_digest(stream,'sha256').hexdigest()
    if digest!=model['prepared_sha256']:raise RuntimeError('SOURCE HASH '+model['id'])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path),use_image_search=False)
    scene=bpy.context.scene;objects=[o for o in scene.objects if o.type=='MESH']
    corners=[o.matrix_world@Vector(c) for o in objects for c in o.bound_box]
    lo=Vector(tuple(min(p[i] for p in corners) for i in range(3)))
    hi=Vector(tuple(max(p[i] for p in corners) for i in range(3)))
    center=(lo+hi)/2;extent=hi-lo
    scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO'
    scene.display.shading.color_type='MATERIAL';scene.render.resolution_x=1000;scene.render.resolution_y=700;scene.render.resolution_percentage=100
    camera_data=bpy.data.cameras.new('InspectionCamera');camera=bpy.data.objects.new('InspectionCamera',camera_data);scene.collection.objects.link(camera)
    scene.camera=camera;camera_data.type='ORTHO';camera_data.ortho_scale=max(extent)*1.28
    entry={'id':model['id'],'source_sha256':digest,'images':[]}
    for label,direction in [('side',Vector((0,-1,0))),('front',Vector((-1,0,0))),('top',Vector((0,0,1)))]:
        camera.location=center+direction*max(extent)*3
        camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
        dest=out/(model['id']+'_'+label+'.png');scene.render.filepath=str(dest);bpy.ops.render.render(write_still=True)
        entry['images'].append(str(dest))
    rows.append(entry);print('OV_WEAPON_ORTHO',model['id'],flush=True)
(out/'report.json').write_text(json.dumps({'scope':'원본 9종 정사영. 접합면/피벗/전장 가동을 확정하지 않음.','models':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
