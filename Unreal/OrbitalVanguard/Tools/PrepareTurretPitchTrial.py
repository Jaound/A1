"""Separate an evidence-selected pitch trial from a yaw trial copy; never alter the input.

The cut and pivot are experimental. Caps, intersections, materials, complete
motion range, muzzle placement, and gameplay must be independently validated.
"""
import argparse,hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils import Matrix,Vector
parser=argparse.ArgumentParser()
for name in ('prefix','evidence','output'):parser.add_argument('--'+name,required=True)
for name in ('cut-x','pivot-x','pivot-y','pivot-z'):parser.add_argument('--'+name,required=True,type=float)
a=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);source=Path(bpy.data.filepath).resolve();evidence=Path(a.evidence).resolve();out=Path(a.output).resolve()
if out.exists() or not source.is_file() or not evidence.is_file():raise RuntimeError('Missing input or output already exists')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);scan=json.loads(evidence.read_text(encoding='utf8'))
if scan.get('source_before')!=before or scan.get('source_after')!=before or scan.get('mesh')!=a.prefix+'_YawTrial' or scan.get('axis')!='x':raise RuntimeError('Mismatched pitch evidence')
original=bpy.data.objects.get(a.prefix+'_YawTrial');base=bpy.data.objects.get(a.prefix+'_BaseTrial')
if not original or not base:raise RuntimeError('Missing yaw trial parts')
pivot=Vector((a.pivot_x,a.pivot_y,a.pivot_z));yaw_pivot=original.matrix_world.translation.copy();out.mkdir(parents=True)
scene=bpy.context.scene;cap=bpy.data.materials.new('TrialPitchCutCap');cap.diffuse_color=(.22,.23,.25,1);parts={};results={}
for suffix,positive in [('YawBody',True),('PitchTrial',False)]:
 mesh=original.data.copy();mesh.name=a.prefix+'_'+suffix+'Mesh';obj=bpy.data.objects.new(a.prefix+'_'+suffix,mesh);scene.collection.objects.link(obj)
 mesh.materials.append(cap);bm=bmesh.new();bm.from_mesh(mesh);bm.transform(original.matrix_world)
 cut=bmesh.ops.bisect_plane(bm,geom=list(bm.verts)+list(bm.edges)+list(bm.faces),dist=1e-5,plane_co=Vector((a.cut_x,0,0)),plane_no=Vector((1,0,0)),clear_inner=positive,clear_outer=not positive)
 edges=[e for e in cut['geom_cut'] if isinstance(e,bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
 filled=bmesh.ops.holes_fill(bm,edges=edges,sides=0) if edges else {'faces':[]}
 for f in filled['faces']:f.material_index=len(mesh.materials)-1
 results[suffix]={'faces':len(bm.faces),'cut_edges':len(edges),'cap_faces':len(filled['faces']),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges)}
 bm.to_mesh(mesh);bm.free();origin=yaw_pivot if positive else pivot;mesh.transform(Matrix.Translation(-origin));obj.location=origin;parts[suffix]=obj
bpy.data.objects.remove(original,do_unlink=True)
body=parts['YawBody'];barrel=parts['PitchTrial'];bpy.context.view_layer.update()
world=barrel.matrix_world.copy();barrel.parent=body;barrel.matrix_world=world;bpy.context.view_layer.update()
cam=bpy.data.cameras.new('PitchTrialCamera');camera=bpy.data.objects.new('PitchTrialCamera',cam);scene.collection.objects.link(camera);scene.camera=camera;cam.type='ORTHO';cam.ortho_scale=5.8
center=Vector((0,0,0));camera.location=Vector((0,-8,1.1));camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL';scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD'
if not scene.world:scene.world=bpy.data.worlds.new('TrialWorld')
scene.world.color=(.025,.035,.05);scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
for angle in (0,15,-15):
 barrel.rotation_euler.y=math.radians(angle);bpy.context.view_layer.update();scene.render.filepath=str(out/('pitch_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
barrel.rotation_euler.y=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.prefix+'_PitchTrial.blend')))
after=sha(source);report={'source':str(source),'source_before':before,'source_after':after,'evidence':str(evidence),'evidence_sha256':sha(evidence),'cut_x':a.cut_x,'trial_pitch_pivot':list(pivot),'trial_yaw_pivot':list(yaw_pivot),'parts':results,'status':'experimental pitch split only; joint, cap, intersections, source material and gameplay unapproved'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('input changed')
print('OV_TURRET_PITCH_TRIAL',out)
