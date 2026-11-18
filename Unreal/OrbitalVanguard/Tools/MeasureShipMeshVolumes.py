"""원본 선체 메시의 닫힘 상태와 기하학적 부피를 조사한다. 원본은 수정하지 않는다."""
from pathlib import Path
import json
import bmesh
import bpy
from mathutils import Vector

root=Path.home()/'Documents/OrbitalVanguardAssets'
out=root/'Reports/ShipMeshVolumes_v1.json'
if out.exists():raise RuntimeError('기존 조사 결과를 덮어쓰지 않습니다')
sources={'corvette':root/'Prepared/PlayUI_R2_PC14_FullMesh_v1/OV_PC14_FullMesh.fbx'}
for tag in ('PlayUI_R2_DestroyerFull_v1','PlayUI_R2_CarrierAssaultFull_v1',
            'PlayUI_R2_MissileDefenseFull_v1','PlayUI_R2_FrigateCruiserRadarFull_v1',
            'PlayUI_R2_SupportShipsFull_v1','PlayUI_R2_HeavyShipsFull_v1'):
    directory=root/'Prepared'/tag
    for item in json.loads((directory/'report.json').read_text(encoding='utf8')):
        ident=item['id'];sources[ident]=directory/ident/('OV_'+ident+'_FullMesh.fbx')
if len(sources)!=16:raise RuntimeError('함체 수 확인 실패')
rows=[]
for ident,source in sources.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source),use_image_search=False)
    objects=[obj for obj in bpy.context.scene.objects if obj.type=='MESH']
    if len(objects)!=1:raise RuntimeError(ident+': 단일 메시가 아님')
    obj=objects[0];bm=bmesh.new();bm.from_mesh(obj.data);bm.transform(obj.matrix_world)
    points=[obj.matrix_world@Vector(corner) for corner in obj.bound_box]
    low=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high=Vector(tuple(max(p[i] for p in points) for i in range(3)))
    size=high-low;box=size.x*size.y*size.z
    nonmanifold=sum(1 for edge in bm.edges if not edge.is_manifold)
    volume=bm.calc_volume(signed=False)
    bm.free()
    rows.append({'id':ident,'bbox_size':list(size),'bbox_volume':box,'mesh_volume':volume,
                 'mesh_to_bbox':volume/box if box>0 else None,'nonmanifold_edges':nonmanifold,
                 'closed_surface':nonmanifold==0})
    out.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
    print('OV_VOLUME',ident,'nonmanifold',nonmanifold,'ratio',rows[-1]['mesh_to_bbox'],flush=True)
