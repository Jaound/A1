"""Sample paired yaw/pitch collisions of three-part trial; input stays unchanged."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils.bvhtree import BVHTree
args=sys.argv[sys.argv.index('--')+1:]
if len(args) not in (2,4):raise ValueError('prefix, report, optionally yaw and pitch steps required')
name,outname=args[:2];yaw_step=int(args[2]) if len(args)==4 else 90;pitch_step=int(args[3]) if len(args)==4 else 0
if yaw_step<=0 or 180%yaw_step or pitch_step<0 or (pitch_step and (15%pitch_step or 90%pitch_step)):raise ValueError('Invalid angular steps')
src=Path(bpy.data.filepath).resolve();out=Path(outname).resolve()
if out.exists():raise FileExistsError(out)
def sha():
 with src.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();base=bpy.data.objects.get(name+'_BaseTrial');body=bpy.data.objects.get(name+'_YawBody');barrel=bpy.data.objects.get(name+'_PitchTrial')
if not base or not body or not barrel:raise RuntimeError('Missing parts')
def tree(obj):
 bm=bmesh.new();bm.from_mesh(obj.data);caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name.startswith(('TrialCutCap','TrialPitchCutCap'))]
 if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
 bm.transform(obj.matrix_world);t=BVHTree.FromBMesh(bm,epsilon=0);bm.free();return t
bpy.context.view_layer.update();base_tree=tree(base);angles=[]
for yaw in range(-180,181,yaw_step):
 body.rotation_euler.z=math.radians(yaw);bpy.context.view_layer.update();body_tree=tree(body);body_base=len(base_tree.overlap(body_tree))
 for pitch in (range(-15,91,pitch_step) if pitch_step else (-15,0,30,60,90)):
  barrel.rotation_euler.y=math.radians(pitch);bpy.context.view_layer.update();pitch_tree=tree(barrel)
  angles.append({'yaw':yaw,'pitch':pitch,'body_base':body_base,'pitch_body':len(body_tree.overlap(pitch_tree)),'pitch_base':len(base_tree.overlap(pitch_tree))})
after=sha();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'source':str(src),'source_before':before,'source_after':after,'angles':angles,'scope':'cap-free top-orientation samples only; not proof of continuous clearance, UE mounting, or gameplay'},ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('input changed')
print('OV_TURRET_COMBINED_INTERSECTIONS',out)
