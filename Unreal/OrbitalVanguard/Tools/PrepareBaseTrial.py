"""기지 네 원본의 UV를 유지한 전장용 메시와 연결면 좌표를 별도로 준비한다. Blender -b -P로 실행."""
import os
import hashlib
import json
from pathlib import Path
import bpy
import numpy as np
from mathutils import Matrix, Vector

root = Path(os.environ.get("OV_BASE_ASSET_ROOT", Path(__file__).resolve().parents[4] / "OrbitalVanguardAssets"))
phase2 = os.environ.get('OV_BASE_STAGE') == '2'
detail = os.environ.get('OV_BASE_DETAIL')
out = Path(detail) / ('Stage2' if phase2 else 'Stage1') if detail else root / ('Prepared/BaseTrial2' if phase2 else 'Prepared/BaseTrial1')
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
if phase2:
    # 단일 선형 배율로 외곽 한도에 맞춘다. 앞면은 조선소 -X, 접안 베이 +Y.
    modules = [
        ('YardSmall','소형 조선소',(16.5,)*3,(0,0,0),
         {'YP':(2.41,-2.0198,.83),'YN':(2.41,2.0198,.83)}),
        ('DockStandard','소형 접안 모듈',(16.65,)*3,(0,0,0),
         {'XN':(-2.7514,0,.999),'XP':(2.7514,-.003,1.05)})]
def fit_octagonal_collars(obj, transform, sockets, name):
    """기존 테두리 안쪽에서 시작해 직선 통로 끝단으로 이어지는 속 빈 금속 칼라."""
    verts, faces = [], []
    # 팔각 단면에서 수직 모서리와 위/아래의 수평 변을 유지한다.
    ring=[(-.55,-1),(.55,-1),(1,-.55),(1,.55),(.55,1),(-.55,1),(-1,.55),(-1,-.55)]
    for socket,point in sockets.items():
        normal=Vector({'XP':(1,0,0),'XN':(-1,0,0),'YP':(0,-1,0),'YN':(0,1,0)}[socket])
        axis=Vector((0,0,1)).cross(normal)
        center=transform@Vector(point)
        wide=(5.4,7.0) if name=='YardSmall' else (6.0,6.5)
        # 실제 직선 통로 외측 끝면 4.36 × 5.35m. 뒤쪽 립은 원본의 굴곡과 겹쳐 뜨지 않게 한다.
        sections=[(-.95,*wide),(.15,*wide), (1.40,2.18,2.68),(1.85,2.18,2.68),
                  (1.85,1.90,2.38),(1.40,1.90,2.38),(.10,wide[0]-.35,wide[1]-.35),(-.95,wide[0]-.35,wide[1]-.35)]
        first=len(verts)
        for depth,w,h in sections:
            for x,z in ring:
                verts.append(center+normal*depth+axis*x*w+Vector((0,0,z*h)))
        for i in range(len(sections)):
            for j in range(8):
                a=first+i*8
                b=first+((i+1)%len(sections))*8
                k=(j+1)%8
                faces.append((a+j,a+k,b+k,b+j))
    geom=bpy.data.meshes.new('PortCollars')
    geom.from_pydata(verts,[],faces)
    geom.update()
    collar=bpy.data.objects.new('PortCollars',geom)
    bpy.context.collection.objects.link(collar)
    mat=bpy.data.materials.new('OV_PortSteel')
    geom.materials.append(mat)
    bpy.ops.object.select_all(action='DESELECT')
    collar.select_set(True)
    obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.join()

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
    if not detail and len(obj.data.polygons)>80000:
        modifier=obj.modifiers.new('전장용 단순화','DECIMATE')
        modifier.ratio=80000/len(obj.data.polygons)
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    if phase2:
        fit_octagonal_collars(obj,transform,sockets,name)
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.ops.export_scene.fbx(filepath=str(dest/(obj.name+'.fbx')),use_selection=True,object_types={'MESH'},
        axis_forward='-Y',axis_up='Z',bake_anim=False,embed_textures=False,path_mode='STRIP')
    for suffix in ([] if detail else ['', '_normal', '_roughness', '_metallic']):
        img=bpy.data.images.load(str(src.with_name(src.stem+suffix+'.png')), check_existing=False)
        img.scale(1024,1024)
        img.filepath_raw=str(dest/('T_'+(suffix.lstrip('_') or 'color')+'.png'))
        img.file_format='PNG'
        img.save()
    offsets={}
    for socket, point in sockets.items():
        p=transform @ Vector(point)
        if phase2:
            p+=Vector({'XP':(1.85,0,0),'XN':(-1.85,0,0),'YP':(0,-1.85,0),'YN':(0,1.85,0)}[socket])
        offsets[socket]=[round(p.x*100,3),round(-p.y*100,3),round(p.z*100,3)]
    with src.open('rb') as stream:
        digest=hashlib.file_digest(stream,'sha256').hexdigest()
    report[name]={'sha256':digest,'source':str(src),'triangles':len(obj.data.polygons),'ports':offsets}
    (out/'ports.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print('BASE_PREPARED',name,flush=True)
