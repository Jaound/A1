"""Create a read-only-input trial with explicit vertical yaw-bearing clearance."""
import hashlib,json,sys
from pathlib import Path
import bpy
args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=3:raise ValueError('prefix, new output directory, vertical offset')
name,outname,raw=args;delta=float(raw);src=Path(bpy.data.filepath).resolve();out=Path(outname).resolve()
if out.exists() or not 0<delta<=.05:raise ValueError('Invalid clearance or output exists')
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(src);body=bpy.data.objects.get(name+'_YawBody');barrel=bpy.data.objects.get(name+'_PitchTrial');base=bpy.data.objects.get(name+'_BaseTrial')
if not body or not barrel or not base or barrel.parent!=body:raise RuntimeError('Missing articulated hierarchy')
bpy.context.view_layer.update();body.location.z+=delta;bpy.context.view_layer.update();out.mkdir(parents=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out/(name+'_ClearanceTrial.blend')));after=sha(src)
report={'source':str(src),'source_before':before,'source_after':after,'clearance_blender_units':delta,'trial_yaw_pivot':list(body.matrix_world.translation),'trial_pitch_pivot':list(barrel.matrix_world.translation),'scope':'explicit bearing gap trial, not final art or gameplay'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('input changed')
print('OV_YAW_CLEARANCE_TRIAL',out)
