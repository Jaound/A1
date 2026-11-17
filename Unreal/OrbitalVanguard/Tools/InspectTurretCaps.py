"""Inspect each trial cut cap in world coordinates without changing the blend."""
import hashlib,json,sys
from pathlib import Path
import bpy
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('part name and new report path required')
obj=bpy.data.objects.get(args[0]);out=Path(args[1]).resolve();src=Path(bpy.data.filepath).resolve()
if not obj or obj.type!='MESH' or out.exists():raise RuntimeError('missing mesh or report exists')
def sha():
 with src.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();caps=[]
for poly in obj.data.polygons:
 if poly.material_index>=len(obj.data.materials):continue
 mat=obj.data.materials[poly.material_index]
 if not mat or mat.name!='TrialCutCap':continue
 points=[obj.matrix_world@obj.data.vertices[i].co for i in poly.vertices]
 caps.append({'vertices':len(points),'area':poly.area,'min':[min(p[i] for p in points) for i in range(3)],'max':[max(p[i] for p in points) for i in range(3)],'vertex_mean':[sum(p[i] for p in points)/len(points) for i in range(3)]})
after=sha();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'source':str(src),'source_before':before,'source_after':after,'part':obj.name,'caps':caps,'scope':'cut-cap evidence only; no authored articulation or pivot approval'},ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('source changed')
print('OV_TURRET_CAP_REPORT',out)
