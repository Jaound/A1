"""Verify a pitch trial hierarchy and report actual body/barrel intersections.

The cap-free BVH samples do not prove continuous-angle clearance or art approval.
"""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils import Matrix,Vector
from mathutils.bvhtree import BVHTree
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('prefix and new report path required')
name=args[0];out=Path(args[1]).resolve();src=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
meta=json.loads((src.parent/'report.json').read_text(encoding='utf8'))
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(src);body=bpy.data.objects.get(name+'_YawBody');barrel=bpy.data.objects.get(name+'_PitchTrial');base=bpy.data.objects.get(name+'_BaseTrial')
if not body or not barrel or not base:raise RuntimeError('Missing trial parts')
checks=[];parts={}
def check(k,v):checks.append({'id':k,'passed':bool(v)})
check('PITCH-PARENT-IS-YAW',barrel.parent==body)
for label,obj in [('BODY',body),('PITCH',barrel)]:
 bm=bmesh.new();bm.from_mesh(obj.data);boundary=sum(e.is_boundary for e in bm.edges);nonmanifold=sum(not e.is_manifold for e in bm.edges);bm.free()
 caps=sum(1 for f in obj.data.polygons if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name=='TrialPitchCutCap')
 parts[label]={'boundary_edges':boundary,'nonmanifold_edges':nonmanifold,'pitch_cap_faces':caps,'uv_layers':[u.name for u in obj.data.uv_layers]}
 check(label+'-CLOSED',boundary==0 and nonmanifold==0);check(label+'-CAP-UV',caps>0 and bool(obj.data.uv_layers))
bpy.context.view_layer.update();pitch_pivot=Vector(meta['trial_pitch_pivot']);yaw_pivot=Vector(meta['trial_yaw_pivot'])
check('ZERO-PITCH-PIVOT',(barrel.matrix_world.translation-pitch_pivot).length<.001);check('ZERO-YAW-PIVOT',(body.matrix_world.translation-yaw_pivot).length<.001)
base0=base.matrix_world.copy();body0=body.matrix_world.copy();barrel0=barrel.matrix_world.copy()
barrel.rotation_euler.y=math.radians(15);bpy.context.view_layer.update()
check('PITCH-PIVOT-STATIONARY',(barrel.matrix_world.translation-pitch_pivot).length<.001);check('PITCH-ACTUALLY-ROTATES',not barrel.matrix_world.to_quaternion().rotation_difference(barrel0.to_quaternion()).angle<.1)
barrel.rotation_euler.y=0;body.rotation_euler.z=math.radians(45);bpy.context.view_layer.update()
expected=yaw_pivot+Matrix.Rotation(math.radians(45),3,'Z')@(pitch_pivot-yaw_pivot)
check('YAW-CARRIES-PITCH-PIVOT',(barrel.matrix_world.translation-expected).length<.001);check('BASE-REMAINS-FIXED',base.matrix_world==base0)
body.rotation_euler.z=0;bpy.context.view_layer.update()
def tree(obj):
 bm=bmesh.new();bm.from_mesh(obj.data)
 caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name in ('TrialPitchCutCap','TrialCutCap')]
 if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
 bm.transform(obj.matrix_world);t=BVHTree.FromBMesh(bm,epsilon=0.0);bm.free();return t
body_tree=tree(body);angles=[]
for angle in (-30,-15,0,15,30):
 barrel.rotation_euler.y=math.radians(angle);bpy.context.view_layer.update();angles.append({'degrees':angle,'surface_intersections':len(body_tree.overlap(tree(barrel)))})
barrel.rotation_euler.y=0;bpy.context.view_layer.update();after=sha(src);check('SOURCE-UNCHANGED',before==after)
report={'passed':all(c['passed'] for c in checks),'checks':checks,'parts':parts,'angles':angles,'source':str(src),'source_before':before,'source_after':after,'scope':'hierarchy/closed meshes and sampled cap-free intersections only; visual joint, materials, continuous range, muzzle and gameplay unapproved'}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('OV_TURRET_PITCH_VALIDATION',report['passed'],out)
