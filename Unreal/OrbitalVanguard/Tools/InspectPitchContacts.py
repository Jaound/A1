"""Locate cap-free pitch trial surface contacts at selected angles; read-only."""
import hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils.bvhtree import BVHTree
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('prefix and new report path required')
name,outname=args;out=Path(outname).resolve();src=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
def sha():
 with src.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();body=bpy.data.objects.get(name+'_YawBody');barrel=bpy.data.objects.get(name+'_PitchTrial')
if not body or not barrel:raise RuntimeError('missing trial parts')
def mesh_and_tree(obj):
 bm=bmesh.new();bm.from_mesh(obj.data)
 caps=[f for f in bm.faces if f.material_index<len(obj.data.materials) and obj.data.materials[f.material_index] and obj.data.materials[f.material_index].name in ('TrialPitchCutCap','TrialCutCap')]
 if caps:bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY')
 bm.transform(obj.matrix_world);bm.faces.ensure_lookup_table();bm.faces.index_update();return bm,BVHTree.FromBMesh(bm,epsilon=0)
bpy.context.view_layer.update();bm_a,tree_a=mesh_and_tree(body);results=[]
for angle in (-30,-15,0,15,30):
 barrel.rotation_euler.y=math.radians(angle);bpy.context.view_layer.update();bm_b,tree_b=mesh_and_tree(barrel);pairs=tree_a.overlap(tree_b)
 centers=[bm_a.faces[i].calc_center_median() for i,j in pairs]; entry={'degrees':angle,'pairs':len(pairs)}
 if centers:entry.update(min=[min(p[i] for p in centers) for i in range(3)],max=[max(p[i] for p in centers) for i in range(3)])
 results.append(entry);bm_b.free()
bm_a.free();after=sha();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'source':str(src),'source_before':before,'source_after':after,'angles':results,'scope':'triangle contact face centers; no clearance or joint approval'},ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('source changed')
print('OV_PITCH_CONTACTS',out)
