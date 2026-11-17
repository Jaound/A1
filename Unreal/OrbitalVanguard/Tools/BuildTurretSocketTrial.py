"""Build an experimental widened recessed socket on a pitch trial COPY.

Only the pitch-cut boundary is moved. The opening is connected to an inset wall
and back cap. The source blend is immutable; art, normals, UVs, interference,
continuous range and gameplay remain unapproved until separately verified.
"""
import argparse,hashlib,json,math,sys
from pathlib import Path
import bmesh,bpy
from mathutils import Vector
parser=argparse.ArgumentParser();parser.add_argument('--prefix',required=True);parser.add_argument('--output',required=True);parser.add_argument('--scale',type=float,required=True);parser.add_argument('--depth',type=float,required=True);parser.add_argument('--flare',type=float,default=1.0)
a=parser.parse_args(sys.argv[sys.argv.index('--')+1:]);src=Path(bpy.data.filepath).resolve();out=Path(a.output).resolve()
if out.exists() or not src.is_file() or not 1<a.scale<=1.5 or not 0<a.depth<=.5 or not 1<=a.flare<=2:raise ValueError('Invalid input or output exists')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(src);old=json.loads((src.parent/'report.json').read_text(encoding='utf8'));body=bpy.data.objects.get(a.prefix+'_YawBody');barrel=bpy.data.objects.get(a.prefix+'_PitchTrial')
if not body or not barrel:raise RuntimeError('Missing pitch trial parts')
mesh=body.data;bm=bmesh.new();bm.from_mesh(mesh)
caps=[f for f in bm.faces if f.material_index<len(mesh.materials) and mesh.materials[f.material_index] and mesh.materials[f.material_index].name=='TrialPitchCutCap']
if len(caps)!=2:raise RuntimeError('Expected two barrel caps; inspect manually')
loops=[(list(f.verts),list(f.edges),f.material_index) for f in caps];bmesh.ops.delete(bm,geom=caps,context='FACES_ONLY');details=[]
for verts,edges,cap_material in loops:
 if not all(e.is_valid and e.is_boundary for e in edges):raise RuntimeError('Cut loop is not an independent boundary')
 cy=(min(v.co.y for v in verts)+max(v.co.y for v in verts))/2;cz=(min(v.co.z for v in verts)+max(v.co.z for v in verts))/2
 for v in verts:v.co.y=cy+(v.co.y-cy)*a.scale;v.co.z=cz+(v.co.z-cz)*a.scale
 extruded=bmesh.ops.extrude_edge_only(bm,edges=edges)['geom'];new_verts=[v for v in extruded if isinstance(v,bmesh.types.BMVert)];new_set=set(new_verts)
 new_edges=[e for e in extruded if isinstance(e,bmesh.types.BMEdge) and all(v in new_set for v in e.verts)]
 if len(new_verts)!=len(verts) or len(new_edges)!=len(edges):raise RuntimeError('Unexpected socket extrusion')
 for v in new_verts:
  v.co.x+=a.depth;v.co.y=cy+(v.co.y-cy)*a.flare;v.co.z=cz+(v.co.z-cz)*a.flare
 filled=bmesh.ops.holes_fill(bm,edges=new_edges,sides=0)['faces']
 if not filled:raise RuntimeError('Socket back failed to close')
 for f in filled:f.material_index=cap_material
 details.append({'rim_vertices':len(verts),'back_faces':len(filled),'center_yz':[cy,cz]})
results={'boundary_edges':sum(e.is_boundary for e in bm.edges),'nonmanifold_edges':sum(not e.is_manifold for e in bm.edges),'faces':len(bm.faces)}
bm.normal_update();bm.to_mesh(mesh);bm.free();mesh.update();out.mkdir(parents=True)
scene=bpy.context.scene
for angle in (0,15,-15,30,-30):
 barrel.rotation_euler.y=math.radians(angle);bpy.context.view_layer.update();scene.render.filepath=str(out/('pitch_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
barrel.rotation_euler.y=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/(a.prefix+'_PitchTrial.blend')))
after=sha(src);report=dict(old);report.update({'source':str(src),'source_before':before,'source_after':after,'socket_scale':a.scale,'socket_depth':a.depth,'socket_flare':a.flare,'socket_loops':details,'socket_topology':results,'status':'experimental widened socket; inspect normals, UV, interference and gameplay before approval'})
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('source changed')
print('OV_TURRET_SOCKET_TRIAL',out)
