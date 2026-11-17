"""Measure paired muzzle rings at the extreme X of a turret trial, without edits."""
import hashlib,json,sys
from pathlib import Path
import bpy
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=3:raise ValueError('part, new report, front-band width required')
name,outname,band=args;band=float(band);src=Path(bpy.data.filepath).resolve();out=Path(outname).resolve();obj=bpy.data.objects.get(name)
if not obj or obj.type!='MESH' or out.exists() or not 0<band<=.1:raise ValueError('Invalid input')
def sha():
 with src.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha();bpy.context.view_layer.update();points=[obj.matrix_world@v.co for v in obj.data.vertices];front=min(p.x for p in points);tip=[p for p in points if p.x<=front+band];groups=[]
for label,side in [('negative_y',-1),('positive_y',1)]:
 pts=[p for p in tip if p.y*side>0]
 if not pts:raise RuntimeError('Muzzle group missing')
 low=[min(p[i] for p in pts) for i in range(3)];high=[max(p[i] for p in pts) for i in range(3)]
 groups.append({'name':label,'vertices':len(pts),'min':low,'max':high,'center':[(low[i]+high[i])/2 for i in range(3)]})
after=sha();out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps({'source':str(src),'source_before':before,'source_after':after,'front_x':front,'band':band,'groups':groups,'scope':'bounds-derived candidate centers only; not muzzle socket or game firing approval'},ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('source changed')
print('OV_TURRET_MUZZLES',out)
