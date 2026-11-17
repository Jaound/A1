"""Count trial base/yaw surface intersections at selected angles, read-only.

Cut-cap faces are omitted so shared flat caps do not dominate. A zero count is
not proof of clearance at every angle or a substitute for an authored rig.
"""
import hashlib
import json
import math
from pathlib import Path
import sys
import bmesh
import bpy
from mathutils.bvhtree import BVHTree

args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('Pass part prefix and a new output report')
name=args[0];out=Path(args[1]).resolve();source=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);base=bpy.data.objects.get(name+'_BaseTrial');yaw=bpy.data.objects.get(name+'_YawTrial')
if not base or not yaw:raise RuntimeError('Missing trial parts')
def tree(obj):
    bm=bmesh.new();bm.from_mesh(obj.data)
    caps=[face for face in bm.faces if face.material_index<len(obj.data.materials) and obj.data.materials[face.material_index]
          and obj.data.materials[face.material_index].name=='TrialCutCap']
    if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
    bm.transform(obj.matrix_world);result=BVHTree.FromBMesh(bm,epsilon=0.0);bm.free();return result
base_tree=tree(base);results=[]
for angle in (-90,-45,0,45,90):
    yaw.rotation_euler.z=math.radians(angle);bpy.context.view_layer.update()
    pairs=base_tree.overlap(tree(yaw));results.append({'degrees':angle,'surface_intersections':len(pairs)})
after=sha(source);report={'source':str(source),'source_before':before,'source_after':after,'angles':results,
 'scope':'surface intersections with caps omitted; no clearance, pitch, material or gameplay approval'}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('Source changed')
print('OV_TURRET_INTERSECTIONS',out)
