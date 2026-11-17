"""Export only the experimental baked UV from a separate MM24 trial blend.

Does not modify or save the input blend. The pitch joint and final art are not
approved; output is for UE inspection only.
"""
import hashlib
import json
from pathlib import Path
import sys
import bpy

args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=1:raise ValueError('Pass a new output directory')
source=Path(bpy.data.filepath).resolve();out=Path(args[0]).resolve()
if not source.is_file():raise FileNotFoundError(source)
if out.exists():raise FileExistsError(out)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);out.mkdir(parents=True);parts=[]
for name in ('MM24_BaseTrial','MM24_YawTrial'):
    original=bpy.data.objects.get(name)
    if not original or not original.data.uv_layers.get('TrialBakeUV'):raise RuntimeError('Missing baked UV '+name)
    obj=original.copy();obj.data=original.data.copy();bpy.context.collection.objects.link(obj)
    for uv in list(obj.data.uv_layers):
        if uv.name!='TrialBakeUV':obj.data.uv_layers.remove(uv)
    if len(obj.data.uv_layers)!=1:raise RuntimeError('Unexpected UV layer count')
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    path=out/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
                             global_scale=1.0,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,bake_anim=False,
                             path_mode='AUTO',embed_textures=False)
    parts.append({'name':name,'fbx':str(path),'sha256':sha(path),'faces':len(obj.data.polygons),
                  'uv_layers':[u.name for u in obj.data.uv_layers]})
    bpy.data.objects.remove(obj,do_unlink=True)
after=sha(source);report={'source':str(source),'source_before':before,'source_after':after,'parts':parts,
                         'scope':'UE inspection trial only; pitch remains fused; art and gameplay unapproved'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('Source blend changed')
print('OV_MM24_UE_EXPORT_TRIAL',out)
