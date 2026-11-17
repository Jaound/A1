"""Trial Boolean-cut twin pitch sockets from a copy of a separated turret.

Clearance dimensions are explicit experimental geometry, not approved art or
weapon performance. Original inputs are hashed and never overwritten.
"""
import argparse,hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils import Vector
parser=argparse.ArgumentParser();parser.add_argument('--prefix',required=True);parser.add_argument('--output',required=True);parser.add_argument('--radius',type=float,required=True);parser.add_argument('--depth',type=float,required=True);parser.add_argument('--center-x',type=float,required=True);parser.add_argument('--center-z',type=float,required=True);parser.add_argument('--centers-y',type=float,nargs=2,required=True);parser.add_argument('--collar-radius',type=float,default=0);parser.add_argument('--collar-depth',type=float);parser.add_argument('--collar-center-x',type=float)
a=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);src=Path(bpy.data.filepath).resolve();out=Path(a.output).resolve()
if out.exists() or not src.is_file() or not .15<=a.radius<=.4 or not .2<=a.depth<=1 or (a.collar_radius and not .1<=a.collar_radius<a.radius) or (a.collar_depth is not None and not .1<=a.collar_depth<=1):raise ValueError('Invalid input or output exists')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(src);old=json.loads((src.parent/'report.json').read_text(encoding='utf8'));body=bpy.data.objects.get(a.prefix+'_YawBody');barrel=bpy.data.objects.get(a.prefix+'_PitchTrial')
if not body or not barrel:raise RuntimeError('Missing pitch trial parts')
collars=[]
if a.collar_radius:
 for i,y in enumerate(a.centers_y):
  collar=body.copy();collar.data=body.data.copy();bpy.context.scene.collection.objects.link(collar);collar.name='TrialSourceCollar_'+str(i)
  bpy.ops.mesh.primitive_cylinder_add(vertices=96,radius=a.collar_radius,depth=a.collar_depth if a.collar_depth is not None else a.depth,location=(a.collar_center_x if a.collar_center_x is not None else a.center_x,y,a.center_z),rotation=(0,math.pi/2,0));cutter=bpy.context.object
  bpy.ops.object.select_all(action='DESELECT');collar.select_set(True);bpy.context.view_layer.objects.active=collar
  mod=collar.modifiers.new('TrialSourceCollar','BOOLEAN');mod.operation='INTERSECT';mod.solver='EXACT';mod.object=cutter
  bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cutter,do_unlink=True)
  if not collar.data.polygons:raise RuntimeError('Source collar extraction produced empty mesh')
  collars.append(collar)
bpy.ops.object.select_all(action='DESELECT');body.select_set(True);bpy.context.view_layer.objects.active=body
for i,y in enumerate(a.centers_y):
 bpy.ops.mesh.primitive_cylinder_add(vertices=96,radius=a.radius,depth=a.depth,location=(a.center_x,y,a.center_z),rotation=(0,math.pi/2,0));cutter=bpy.context.object;cutter.name='TrialSocketCutter_'+str(i)
 bpy.ops.object.select_all(action='DESELECT');body.select_set(True);bpy.context.view_layer.objects.active=body
 mod=body.modifiers.new('TrialBooleanSocket_'+str(i),'BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter
 bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cutter,do_unlink=True)
if collars:
 bpy.ops.object.select_all(action='DESELECT');barrel.select_set(True)
 for collar in collars:collar.select_set(True)
 bpy.context.view_layer.objects.active=barrel;bpy.ops.object.join()
mesh=body.data;bm=bmesh.new();bm.from_mesh(mesh);results={'faces':len(bm.faces),'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges)};bm.free();out.mkdir(parents=True)
scene=bpy.context.scene
for angle in (0,15,-15,30,-30):
 barrel.rotation_euler.y=math.radians(angle);bpy.context.view_layer.update();scene.render.filepath=str(out/('pitch_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
barrel.rotation_euler.y=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.prefix+'_PitchTrial.blend')))
after=sha(src);report=dict(old);report.update({'source':str(src),'source_before':before,'source_after':after,'socket_radius':a.radius,'socket_depth':a.depth,'collar_radius':a.collar_radius,'collar_depth':a.collar_depth,'collar_center_x':a.collar_center_x,'socket_center_x':a.center_x,'socket_center_z':a.center_z,'socket_centers_y':a.centers_y,'socket_topology':results,'status':'experimental boolean clearance; inspect topology, UV, normals, appearance, interference and gameplay before approval'})
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('source changed')
print('OV_TURRET_BOOLEAN_SOCKET',out)
