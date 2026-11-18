"""2D 색상 후보를 원본 메시 표면에 투영한다. 후보만 기록하며 게임을 변경하지 않는다."""
from pathlib import Path
import json
import bpy
from mathutils import Vector

root=Path.home()/'Documents/OrbitalVanguardAssets'
guides=root/'WeaponPlacementGuide_v1'
marks=json.loads((guides/'표시_영역_후보_v1.json').read_text(encoding='utf8'))['ships']
views=json.loads((guides/'views/report.json').read_text(encoding='utf8'))
output=guides/'3D_표면_후보_v1.json'
if output.exists():raise RuntimeError('기존 결과 보존')
names={'초계함':'corvette','호위함':'frigate','백터급 구축함':'vector','바스티온급 구축함':'bastion',
       '순양함':'cruiser','방공함':'air_defense','레이더함':'radar','전함':'battleship','항공모함':'carrier',
       '전략 미사일함':'missile_ship','구축전함':'dreadnought','항공순양함':'battlecarrier',
       '보급수리함':'repair','채굴선':'mining','수송선':'transport','강습선':'assault'}
sources={'corvette':root/'Prepared/PlayUI_R2_PC14_FullMesh_v1/OV_PC14_FullMesh.fbx'}
for tag in ('PlayUI_R2_DestroyerFull_v1','PlayUI_R2_CarrierAssaultFull_v1','PlayUI_R2_MissileDefenseFull_v1',
            'PlayUI_R2_FrigateCruiserRadarFull_v1','PlayUI_R2_SupportShipsFull_v1','PlayUI_R2_HeavyShipsFull_v1'):
    d=root/'Prepared'/tag
    for entry in json.loads((d/'report.json').read_text(encoding='utf8')):
        ident=entry['id'];sources[ident]=d/ident/('OV_'+ident+'_FullMesh.fbx')
panels={'top':(20,170),'bottom':(930,170),'left':(20,710),'right':(930,710)}
directions={'top':Vector((0,0,1)),'bottom':Vector((0,0,-1)),
            'left':Vector((0,1,0)),'right':Vector((0,-1,0))}
rows=[]
for mark in marks:
    ident=names[Path(mark['file']).stem]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(sources[ident]),use_image_search=False)
    objects=[o for o in bpy.context.scene.objects if o.type=='MESH']
    if len(objects)!=1:raise RuntimeError(ident+': 메시 수 오류')
    obj=objects[0];points=[obj.matrix_world@Vector(c) for c in obj.bound_box]
    low=Vector(tuple(min(p[i] for p in points) for i in range(3)))
    high=Vector(tuple(max(p[i] for p in points) for i in range(3)))
    center=(low+high)/2;size=high-low
    fit=min(350/size.x,150/size.y,60/size.z)
    ortho=max(size.x*1.28,size.y*2,size.z*2)
    depsgraph=bpy.context.evaluated_depsgraph_get();candidates=[]
    for view,colors in mark['views'].items():
        direction=directions[view];rotation=(-direction).to_track_quat('-Z','Y').to_matrix()
        right=rotation@Vector((1,0,0));up=rotation@Vector((0,1,0))
        flipped=next(v['flip_x'] for v in views if v['id']==ident and v['view']==view)
        x0,y0=panels[view]
        for color,regions in colors.items():
            for index,region in enumerate(regions):
                px,py=region['center_px'];screen_x=(px-x0)/900-.5;screen_y=.5-(py-y0)/500
                if flipped:screen_x=-screen_x
                origin=center+direction*max(size)*3+right*(screen_x*ortho)+up*(screen_y*ortho*500/900)
                hit,location,normal,face,_,_=bpy.context.scene.ray_cast(depsgraph,origin,-direction,distance=max(size)*10)
                pos=location-center if hit else Vector()
                local=[-pos.x*fit,-pos.y*fit,pos.z*fit] if hit else None
                candidates.append({'view':view,'color':color,'region':index,'pixel':region['center_px'],
                                   'hit':bool(hit),'local_cm':local,'normal':[-normal.x,-normal.y,normal.z] if hit else None})
    rows.append({'id':ident,'marked_sha256':mark['sha256'],'candidates':candidates})
    output.write_text(json.dumps({'scope':'표면 투영 후보; 슬롯 식별·중복·충돌·가동 미검증','ships':rows},ensure_ascii=False,indent=2),encoding='utf8')
    print('OV_PROJECT',ident,len(candidates),sum(x['hit'] for x in candidates),flush=True)
