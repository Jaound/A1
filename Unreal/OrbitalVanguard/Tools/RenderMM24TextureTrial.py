"""Render read-only textured MM24 trial views; pass a new output folder after --."""
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy

args=sys.argv[sys.argv.index('--')+1:]
baked='--baked' in args
args=[a for a in args if a!='--baked']
prefix=next((a.split('=',1)[1] for a in args if a.startswith('--prefix=')),'MM24')
args=[a for a in args if not a.startswith('--prefix=')]
if not prefix.replace('_','').isalnum():raise ValueError('Invalid part prefix')
if len(args)!=1:raise ValueError('Pass a new output folder after --')
source=Path(bpy.data.filepath).resolve();out=Path(args[0]).resolve()
if out.exists():raise FileExistsError(out)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);out.mkdir(parents=True)
for name in (prefix+'_BaseTrial',prefix+'_YawTrial'):
    obj=bpy.data.objects.get(name)
    if not obj:raise RuntimeError('Missing '+name)
    for mat in obj.data.materials:
        if mat and mat.use_nodes:
            nodes=mat.node_tree.nodes
            color=next((n for n in nodes if n.type=='TEX_IMAGE' and n.image and
                        (n.image.name.startswith(prefix+'_TrialBake') if baked else n.image.name=='Image_0')),None)
            if color:nodes.active=color
scene=bpy.context.scene;scene.display.shading.color_type='TEXTURE';scene.render.image_settings.file_format='PNG'
yaw=bpy.data.objects[prefix+'_YawTrial']
for angle in (0,45,-45):
    yaw.rotation_euler.z=math.radians(angle)
    scene.render.filepath=str(out/('textured_yaw_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
after=sha(source)
(out/'report.json').write_text(json.dumps({'source_blend':str(source),'source_sha256_before':before,'source_sha256_after':after,
    'source_unchanged':before==after,'baked':baked,'scope':'Workbench color comparison only; no lighting, UV, pitch or gameplay approval'},ensure_ascii=False,indent=2),encoding='utf-8')
if before!=after:raise RuntimeError('Source blend unexpectedly changed')
print('OV_MM24_TEXTURE_VIEWS',out)
