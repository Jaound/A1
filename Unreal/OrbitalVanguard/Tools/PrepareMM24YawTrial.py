"""Experimental MM24 base/yaw split at an inspected seam; never edits the FBX.

The cut height is a trial from orthographic images and slice measurements, not
an authored rig. Pitch separation, UE import and art approval remain outstanding.
Usage: blender --background --python PrepareMM24YawTrial.py -- --source <fbx> --output <new-folder>
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

parser=argparse.ArgumentParser();parser.add_argument('--source',required=True);parser.add_argument('--output',required=True)
parser.add_argument('--cut-z',type=float,default=-.975)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
source=Path(args.source).resolve();out=Path(args.output).resolve()
if not source.is_file():raise FileNotFoundError(source)
if out.exists():raise FileExistsError(out)
if not -1.0<=args.cut_z<=-.95:raise ValueError('Trial cut must stay inside the inspected pivot band')
def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=digest(source);out.mkdir(parents=True)
bpy.ops.wm.read_factory_settings(use_empty=True);bpy.ops.import_scene.fbx(filepath=str(source))
meshes=[obj for obj in bpy.context.scene.objects if obj.type=='MESH']
if len(meshes)!=1:raise RuntimeError('Expected exactly one original MM24 mesh')
original=meshes[0];original.data.transform(original.matrix_world);original.matrix_world=Matrix.Identity(4)
scene=bpy.context.scene;pivot=Vector((0,0,args.cut_z))
cap=bpy.data.materials.new('TrialCutCap');cap.diffuse_color=(.22,.23,.25,1)
results={};parts=[]
for name,upper in [('MM24_BaseTrial',False),('MM24_YawTrial',True)]:
    mesh=original.data.copy();mesh.name=name+'Mesh';obj=bpy.data.objects.new(name,mesh);scene.collection.objects.link(obj)
    mesh.materials.append(cap);bm=bmesh.new();bm.from_mesh(mesh)
    geometry=list(bm.verts)+list(bm.edges)+list(bm.faces)
    cut=bmesh.ops.bisect_plane(bm,geom=geometry,dist=1e-5,plane_co=pivot,plane_no=Vector((0,0,1)),clear_inner=upper,clear_outer=not upper)
    cut_edges=[e for e in cut['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
    filled=bmesh.ops.holes_fill(bm,edges=cut_edges,sides=0) if cut_edges else {'faces':[]}
    for face in filled['faces']:face.material_index=len(mesh.materials)-1
    boundary=sum(1 for e in bm.edges if e.is_boundary);nonmanifold=sum(1 for e in bm.edges if not e.is_manifold)
    results[name]={'vertices':len(bm.verts),'faces':len(bm.faces),'cut_boundary_edges':len(cut_edges),
                   'generated_cap_faces':len(filled['faces']),'remaining_boundary_edges':boundary,'nonmanifold_edges':nonmanifold}
    bm.to_mesh(mesh);bm.free();mesh.transform(Matrix.Translation(-pivot));obj.location=pivot
    results[name]['uv_layers']=[layer.name for layer in mesh.uv_layers]
    parts.append(obj)
bpy.data.objects.remove(original,do_unlink=True)
camera=bpy.data.cameras.new('TrialCamera');camera_obj=bpy.data.objects.new('TrialCamera',camera);scene.collection.objects.link(camera_obj)
scene.camera=camera_obj;camera.type='ORTHO';camera.ortho_scale=3.8
camera_obj.location=Vector((5,-6,4));camera_obj.rotation_euler=(Vector((0,0,0))-camera_obj.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD';scene.world=bpy.data.worlds.new('TrialWorld');scene.world.color=(.025,.035,.05)
scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100;scene.render.image_settings.file_format='PNG'
for angle in (0,45,-45):
    parts[1].rotation_euler.z=angle*3.141592653589793/180
    scene.render.filepath=str(out/('yaw_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
parts[1].rotation_euler.z=0
bpy.ops.wm.save_as_mainfile(filepath=str(out/'MM24_YawTrial.blend'))
after=digest(source)
report={'source':str(source),'source_sha256_before':before,'source_sha256_after':after,'source_unchanged':before==after,
        'cut_z_blender':args.cut_z,'evidence':'orthographic views and 60 vertical vertex slices around the lower rotating ring',
        'parts':results,'status':'experimental base/yaw cut only; caps, gaps, intersections, UV and mechanical pivot require review; pitch not separated'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if before!=after:raise RuntimeError('Source unexpectedly changed')
print('OV_MM24_YAW_TRIAL',out)
