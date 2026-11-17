"""Create a reversible, evidence-linked base/yaw split of a fused FBX copy.

The source is never saved. The selected plane is an experimental seam, not
an assertion that the source contains an authored rig. Pitch, caps, contact,
visual fidelity, collision and gameplay require independent verification.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys
import bmesh
import bpy
from mathutils import Matrix,Vector

parser=argparse.ArgumentParser()
for name in ('source','output','evidence','name'):parser.add_argument('--'+name,required=True)
parser.add_argument('--cut-z',required=True,type=float)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
source=Path(args.source).resolve();out=Path(args.output).resolve();evidence=Path(args.evidence).resolve()
if not source.is_file() or not evidence.is_file():raise FileNotFoundError('Source or inspection report missing')
if out.exists():raise FileExistsError(out)
if not args.name.replace('_','').isalnum():raise ValueError('Invalid asset identifier')
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
inspection=json.loads(evidence.read_text(encoding='utf8'));before=sha(source)
if not inspection.get('source_unchanged') or inspection.get('source_sha256_before')!=before:
    raise ValueError('Inspection report does not match unchanged source')
zmin,zmax=inspection['bounds_min'][2],inspection['bounds_max'][2]
if not zmin<args.cut_z<zmax:raise ValueError('Cut is outside inspected bounds')
out.mkdir(parents=True);bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(source))
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
if len(meshes)!=1:raise RuntimeError('Expected one fused mesh; inspect before attempting split')
original=meshes[0];original.data.transform(original.matrix_world);original.matrix_world=Matrix.Identity(4)
scene=bpy.context.scene;pivot=Vector((0,0,args.cut_z));cap=bpy.data.materials.new('TrialCutCap');cap.diffuse_color=(.22,.23,.25,1)
parts=[];results={}
for suffix,upper in [('BaseTrial',False),('YawTrial',True)]:
    name=args.name+'_'+suffix;mesh=original.data.copy();mesh.name=name+'Mesh';obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj)
    mesh.materials.append(cap);bm=bmesh.new();bm.from_mesh(mesh)
    cut=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=1e-5,plane_co=pivot,plane_no=Vector((0,0,1)),clear_inner=upper,clear_outer=not upper)
    edges=[e for e in cut['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
    filled=bmesh.ops.holes_fill(bm,edges=edges,sides=0) if edges else {'faces':[]}
    for face in filled['faces']:face.material_index=len(mesh.materials)-1
    results[name]={'faces':len(bm.faces),'cut_edges':len(edges),'cap_faces':len(filled['faces']),
                   'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges)}
    bm.to_mesh(mesh);bm.free();mesh.transform(Matrix.Translation(-pivot));obj.location=pivot
    results[name]['uv_layers']=[u.name for u in mesh.uv_layers];parts.append(obj)
bpy.data.objects.remove(original,do_unlink=True)
cam=bpy.data.cameras.new('TrialCamera');camera=bpy.data.objects.new('TrialCamera',cam);scene.collection.objects.link(camera)
scene.camera=camera;cam.type='ORTHO';cam.ortho_scale=max(inspection['bounds_max'][i]-inspection['bounds_min'][i] for i in range(3))*1.4
camera.location=Vector((5,-6,4));camera.rotation_euler=(-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD'
if not scene.world:scene.world=bpy.data.worlds.new('TrialWorld')
scene.world.color=(.025,.035,.05);scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
for angle in (0,45,-45):
    parts[1].rotation_euler.z=math.radians(angle);scene.render.filepath=str(out/('yaw_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
parts[1].rotation_euler.z=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(args.name+'_YawTrial.blend')))
after=sha(source);report={'source':str(source),'source_before':before,'source_after':after,'evidence':str(evidence),
  'evidence_sha256':sha(evidence),'cut_z':args.cut_z,'parts':results,
  'status':'experimental base/yaw only; pitch, cap, pivot, visual and gameplay unapproved'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('Source unexpectedly changed')
print('OV_TURRET_YAW_TRIAL',out)
