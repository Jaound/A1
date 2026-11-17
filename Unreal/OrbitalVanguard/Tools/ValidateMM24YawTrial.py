"""Read-only geometric checks for an already prepared MM24 yaw trial blend.

Run with the trial blend loaded and pass a new JSON path after --. The z-plane
test cannot certify lateral contact, visual quality, pitch, or Unreal gameplay.
"""
import hashlib
import json
import math
from pathlib import Path
import sys

import bmesh
import bpy
from mathutils import Vector

args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=1:raise ValueError('Pass a new report path after --')
out=Path(args[0]).resolve()
if out.exists():raise FileExistsError(out)
blend=Path(bpy.data.filepath);prior=json.loads((blend.parent/'report.json').read_text(encoding='utf-8'))
def sha(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
base=bpy.data.objects.get('MM24_BaseTrial');yaw=bpy.data.objects.get('MM24_YawTrial')
if not base or not yaw:raise RuntimeError('Trial parts missing')
parts={}
for obj in (base,yaw):
    bm=bmesh.new();bm.from_mesh(obj.data)
    parts[obj.name]={'boundary_edges':sum(1 for e in bm.edges if e.is_boundary),
                     'nonmanifold_edges':sum(1 for e in bm.edges if not e.is_manifold),
                     'uv_layers':[x.name for x in obj.data.uv_layers],
                     'cap_faces':sum(1 for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name=='TrialCutCap')}
    bm.free()
angles=[]
for degrees in (-180,-90,-45,0,45,90,180):
    yaw.rotation_euler.z=math.radians(degrees);bpy.context.view_layer.update()
    bz=max((base.matrix_world@Vector(c)).z for c in base.bound_box)
    yz=min((yaw.matrix_world@Vector(c)).z for c in yaw.bound_box)
    angles.append({'yaw_degrees':degrees,'base_max_z':bz,'yaw_min_z':yz,'vertical_gap':yz-bz})
images=[im for im in bpy.data.images if im.source=='FILE']
source_unchanged=sha(prior['source'])==prior['source_sha256_before']
passed=(source_unchanged and all(p['boundary_edges']==0 and p['nonmanifold_edges']==0 and p['uv_layers'] and p['cap_faces']>0 for p in parts.values())
        and all(abs(a['vertical_gap'])<1e-4 for a in angles) and bool(images) and all(im.packed_file for im in images))
report={'passed':bool(passed),'source_unchanged':source_unchanged,'parts':parts,'angles':angles,
        'packed_images':[im.name for im in images if im.packed_file],
        'limitations':['Cap material is provisional; UV quality is not certified',
                       'Coincident z boundaries do not prove lateral fit or visual acceptance',
                       'Pitch remains fused; UE import and gameplay are not tested']}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('OV_MM24_YAW_VALIDATION',passed,str(out))
if not passed:raise RuntimeError('Trial geometric validation failed')
