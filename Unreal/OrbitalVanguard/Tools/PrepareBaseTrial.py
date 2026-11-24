"""기지 네 원본의 UV를 유지한 전장용 메시와 연결면 좌표를 별도로 준비한다. Blender -b -P로 실행."""
import hashlib
import json
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Vector

root = Path.home() / 'Documents/OrbitalVanguardAssets'
out = root / 'Prepared/BaseTrial1'
out.mkdir(parents=True, exist_ok=True)
# Blender의 +Y는 UE의 -Y. 원본 투영에서 확인한 노출면의 중심(원본 월드 m).
# 코어 상단 안테나는 연결구가 아니다. 수직 분기는 허브에서 한다.
modules = [
    ('CoreHub_5x5x5', '기지 코어 모듈', (6.14,6.14,4.7), (0,.14,10.57),
     {'XP':(10.18,.149,10.57),'XN':(-10.18,.132,10.568),'YP':(-.028,-10.18,10.42),'YN':(0,10.18,10.33)}),
    ('CorridorStraight_1x1x1','일자 통로 모듈',(1.812,1.812,1.812),(0,0,2.005),
     {'XN':(-6.8996,-.003,2.006),'XP':(6.8996,.003,2.004)}),
    ('CorridorHub_1x1x1','사거리 통로 모듈',(4.869,4.869,8.333),(0,0,1.501),
     {'XN':(-2.567,-.002,1.501),'XP':(2.567,-.002,1.502),'YP':(-.002,-2.568,1.502),
      'YN':(-.001,2.568,1.501),'ZP':(.014,.003,3),'ZN':(-.015,-.001,0)}),
    ('StorageVault','자원 저장소',(3.2,3.2,3.2),(-7.271,.958,1.954),
     {'XN':(-7.271,.958,1.954)})]
report = {}
for name, directory, scale, pivot, sockets in modules:
    src = next((root/'Source/Base'/directory).rglob('*.fbx'))
    dest = out/name
    dest.mkdir(exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(src), use_image_search=False)
    obj, = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    transform = Matrix.Diagonal(Vector((*scale,1))) @ Matrix.Translation(-Vector(pivot))
    obj.data.transform(transform @ obj.matrix_world)
    obj.matrix_world = Matrix.Identity(4)
    obj.name = 'SM_OV_Base_'+name
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    if len(obj.data.polygons)>80000:
        modifier=obj.modifiers.new('전장용 단순화','DECIMATE')
        modifier.ratio=80000/len(obj.data.polygons)
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.ops.export_scene.fbx(filepath=str(dest/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},
        axis_forward='-Y',axis_up='Z',bake_anim=False,embed_textures=False,path_mode='STRIP')
    for suffix in ['', '_normal', '_roughness', '_metallic']:
        img=bpy.data.images.load(str(src.with_name(src.stem+suffix+'.png')), check_existing=False)
        img.scale(1024,1024)
        img.filepath_raw=str(dest/('T_'+(suffix.lstrip('_') or 'color')+'.png'))
        img.file_format='PNG'
        img.save()
    offsets={}
    for socket, point in sockets.items():
        p=transform @ Vector(point)
        offsets[socket]=[round(p.x*100,3),round(-p.y*100,3),round(p.z*100,3)]
    with src.open('rb') as stream:
        digest=hashlib.file_digest(stream,'sha256').hexdigest()
    report[name]={'sha256':digest,'triangles':len(obj.data.polygons),'ports':offsets}
    (out/'ports.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print('BASE_PREPARED',name,flush=True)
