"""Read-only checks for an MM24 low-poly trial; does not certify art or gameplay."""
import hashlib
import json
import math
from pathlib import Path
import sys

import bmesh
import bpy
from mathutils import Vector

args=sys.argv[sys.argv.index('--')+1:]
if len(args) not in (1,2):raise ValueError('Pass a new report path and optional part prefix after --')
out=Path(args[0]).resolve();prefix=args[1] if len(args)==2 else 'MM24'
if out.exists():raise FileExistsError(out)
blend=Path(bpy.data.filepath);prior=json.loads((blend.parent/'report.json').read_text(encoding='utf-8'))
def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
parts={};checks=[]
def check(name,value):checks.append({'id':name,'passed':bool(value)})
for name in (prefix+'_BaseTrial',prefix+'_YawTrial'):
    obj=bpy.data.objects.get(name)
    if not obj:raise RuntimeError('Missing '+name)
    bm=bmesh.new();bm.from_mesh(obj.data)
    item={'faces':len(obj.data.polygons),'boundary_edges':sum(1 for e in bm.edges if e.is_boundary),
          'nonmanifold_edges':sum(1 for e in bm.edges if not e.is_manifold),
          'uv_layers':[x.name for x in obj.data.uv_layers],'custom_normals':bool(obj.data.has_custom_normals),
          'cap_faces':sum(1 for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name=='TrialCutCap')}
    bm.free();parts[name]=item
    check(name+'-TOPOLOGY',item['boundary_edges']==0 and item['nonmanifold_edges']==0)
    check(name+'-UV-NORMAL-CAP',bool(item['uv_layers']) and item['custom_normals'] and item['cap_faces']>0)
    check(name+'-FACE-BUDGET',item['faces']<=prior['parts'][name]['target_faces']*1.05)
    check(name+'-FBX-HASH',sha(blend.parent/(name+'.fbx'))==prior['parts'][name]['fbx_sha256'])
    for tex in prior['extracted_packed_textures']:
        copied=blend.parent/(name+'.fbm')/tex['name']
        check(name+'-TEXTURE-'+tex['name'],copied.is_file() and sha(copied)==tex['sha256'])
base=bpy.data.objects[prefix+'_BaseTrial'];yaw=bpy.data.objects[prefix+'_YawTrial'];angles=[]
for degrees in (-180,-90,-45,0,45,90,180):
    yaw.rotation_euler.z=math.radians(degrees);bpy.context.view_layer.update()
    gap=min((yaw.matrix_world@Vector(c)).z for c in yaw.bound_box)-max((base.matrix_world@Vector(c)).z for c in base.bound_box)
    angles.append({'yaw_degrees':degrees,'vertical_gap':gap})
    check('SEAM-Z-'+str(degrees),abs(gap)<1e-3)
check('SOURCE-BLEND-UNCHANGED',sha(prior['source_blend'])==prior['source_sha256_before'])
images=[im for im in bpy.data.images if im.source=='FILE'];check('PACKED-IMAGES',len(images)>=4 and all(im.packed_file for im in images))
report={'passed':all(c['passed'] for c in checks),'checks':checks,'parts':parts,'angles':angles,
        'limitations':['Contact z bounds do not establish lateral collision clearance',
                       'Source-map visual quality, provisional caps and custom-normal appearance require review',
                       'Pitch remains fused and Unreal import, gameplay and performance are not verified']}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('OV_MM24_LIGHT_VALIDATION',report['passed'],str(out))
if not report['passed']:raise RuntimeError('Low-poly trial validation failed; see report')
