"""Read-only topology and vertical-seam checks for a named trial split.

This does not verify lateral contact, intersections, pitch, art or gameplay.
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
if len(args)!=2:raise ValueError('Pass part prefix and a new report path')
name=args[0];out=Path(args[1]).resolve();blend=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
prior=json.loads((blend.parent/'report.json').read_text(encoding='utf8'))
def sha(p):
    with Path(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
checks=[];parts={}
def check(label,value):checks.append({'id':label,'passed':bool(value)})
for suffix in ('BaseTrial','YawTrial'):
    obj=bpy.data.objects.get(name+'_'+suffix)
    if not obj:raise RuntimeError('Missing part '+suffix)
    bm=bmesh.new();bm.from_mesh(obj.data)
    parts[suffix]={'faces':len(obj.data.polygons),'boundary_edges':sum(e.is_boundary for e in bm.edges),
                   'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),
                   'uv_layers':[u.name for u in obj.data.uv_layers],
                   'cap_faces':sum(1 for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name=='TrialCutCap')}
    bm.free();v=parts[suffix]
    check(suffix+'-TOPOLOGY',v['boundary_edges']==0 and v['nonmanifold_edges']==0)
    check(suffix+'-UV-CAP',bool(v['uv_layers']) and v['cap_faces']>0)
base=bpy.data.objects[name+'_BaseTrial'];yaw=bpy.data.objects[name+'_YawTrial'];angles=[]
for angle in (-180,-90,-45,0,45,90,180):
    yaw.rotation_euler.z=math.radians(angle);bpy.context.view_layer.update()
    gap=min((yaw.matrix_world@Vector(c)).z for c in yaw.bound_box)-max((base.matrix_world@Vector(c)).z for c in base.bound_box)
    angles.append({'degrees':angle,'vertical_gap':gap});check('SEAM-Z-'+str(angle),abs(gap)<1e-3)
check('ORIGINAL-SOURCE-UNCHANGED',sha(prior['source'])==prior['source_before']==prior['source_after'])
images=[im for im in bpy.data.images if im.source=='FILE'];check('PACKED-IMAGES',len(images)>=4 and all(im.packed_file for im in images))
report={'passed':all(c['passed'] for c in checks),'checks':checks,'parts':parts,'angles':angles,
        'scope':'structural trial only; lateral fit, intersections, art and gameplay unverified'}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
print('OV_TURRET_YAW_VALIDATION',report['passed'],out)
if not report['passed']:raise RuntimeError('Trial validation failed; inspect report')
