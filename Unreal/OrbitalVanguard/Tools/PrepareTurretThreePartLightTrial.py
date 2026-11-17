"""Decimate a three-part turret trial into a new blend; keep input unchanged.

This is not art approval. Source textures, new Boolean surfaces, topology,
clearance, normals and UVs must be inspected independently before use.
"""
import argparse,hashlib,json,math,sys
from pathlib import Path
import bpy,bmesh
parser=argparse.ArgumentParser();parser.add_argument('--prefix',required=True);parser.add_argument('--output',required=True);parser.add_argument('--base-faces',type=int,required=True);parser.add_argument('--body-faces',type=int,required=True);parser.add_argument('--pitch-faces',type=int,required=True);parser.add_argument('--weighted-normals',action='store_true')
a=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);src=Path(bpy.data.filepath).resolve();out=Path(a.output).resolve()
if out.exists() or not src.is_file() or min(a.base_faces,a.body_faces,a.pitch_faces)<=0:raise ValueError('Invalid input or output exists')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(src);meta=json.loads((src.parent/'report.json').read_text(encoding='utf8'));out.mkdir(parents=True);textures=[];results={}
for img in bpy.data.images:
 if img.packed_file:
  suffix=Path(img.filepath).suffix.lower() or '.png';name=img.name if img.name.lower().endswith(suffix) else img.name+suffix;target=out/'Textures'/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(bytes(img.packed_file.data));textures.append({'name':name,'sha256':sha(target),'bytes':target.stat().st_size})
for suffix,target in [('BaseTrial',a.base_faces),('YawBody',a.body_faces),('PitchTrial',a.pitch_faces)]:
 obj=bpy.data.objects.get(a.prefix+'_'+suffix)
 if not obj or obj.type!='MESH':raise RuntimeError('Missing part '+suffix)
 n=len(obj.data.polygons);bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
 dec=obj.modifiers.new('TrialDecimate','DECIMATE');dec.decimate_type='COLLAPSE';dec.ratio=min(1,target/max(1,n));dec.use_collapse_triangulate=True;bpy.ops.object.modifier_apply(modifier=dec.name)
 if a.weighted_normals:
  normals=bmesh.new();normals.from_mesh(obj.data);normals.normal_update()
  for edge in normals.edges:
   if edge.is_manifold and edge.calc_face_angle(0)>math.radians(28):edge.smooth=False
  normals.to_mesh(obj.data);normals.free();mod=obj.modifiers.new('TrialWeightedNormals','WEIGHTED_NORMAL');mod.keep_sharp=True;bpy.ops.object.modifier_apply(modifier=mod.name)
 bm=bmesh.new();bm.from_mesh(obj.data);boundary=sum(e.is_boundary for e in bm.edges);nonmanifold=sum(not e.is_manifold for e in bm.edges);bm.free()
 results[suffix]={'source_faces':n,'target_faces':target,'output_faces':len(obj.data.polygons),'boundary_edges':boundary,'nonmanifold_edges':nonmanifold,'uv_layers':[u.name for u in obj.data.uv_layers],'weighted_normals':a.weighted_normals}
body=bpy.data.objects[a.prefix+'_YawBody'];barrel=bpy.data.objects[a.prefix+'_PitchTrial'];scene=bpy.context.scene
for angle in (0,15,-15,30,-30):
 barrel.rotation_euler.y=angle*3.141592653589793/180;bpy.context.view_layer.update();scene.render.filepath=str(out/('pitch_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
barrel.rotation_euler.y=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.prefix+'_LightTrial.blend')))
after=sha(src);report=dict(meta);report.update({'source':str(src),'source_before':before,'source_after':after,'light_parts':results,'textures':textures,'status':'three-part low-poly experiment; topology, UV, material, clearance and gameplay unapproved'})
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('input changed')
print('OV_THREE_PART_LIGHT_TRIAL',out)
