"""Sample actual yaw-body/base intersections in a saved three-part turret trial."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils.bvhtree import BVHTree
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('prefix and new report path required')
name,outname=args;src=Path(bpy.data.filepath).resolve();out=Path(outname).resolve()
if out.exists():raise FileExistsError(out)
def sha():
 with src.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();base=bpy.data.objects.get(name+'_BaseTrial');body=bpy.data.objects.get(name+'_YawBody')
if not base or not body:raise RuntimeError('Missing parts')
def tree(obj):
 bm=bmesh.new();bm.from_mesh(obj.data);caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name.startswith(('TrialCutCap','TrialPitchCutCap'))]
 if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
 bm.transform(obj.matrix_world);t=BVHTree.FromBMesh(bm,epsilon=0);bm.free();return t
bpy.context.view_layer.update();base_tree=tree(base);angles=[]
for angle in range(-180,181,15):
 body.rotation_euler.z=math.radians(angle);bpy.context.view_layer.update();angles.append({'degrees':angle,'surface_intersections':len(base_tree.overlap(tree(body)))})
after=sha();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'source':str(src),'source_before':before,'source_after':after,'angles':angles,'scope':'cap-free yaw-body/base surface contacts, sampled angles only; zero-degree contact may be coplanar'},ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('input changed')
print('OV_TURRET_YAW_BODY',out)
