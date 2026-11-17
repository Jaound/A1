"""Bake original colour onto a separate MM24 low-poly trial; never modifies inputs.

This is a visual experiment, not gameplay or art approval. The pitch joint remains
fused. Run in Blender with the low-poly .blend open and pass high-poly .blend and
a new output directory after --.
"""
import hashlib
import json
from pathlib import Path
import sys

import bpy

args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('Pass the high-poly .blend and a new output directory')
high=Path(args[0]).resolve();low=Path(bpy.data.filepath).resolve();out=Path(args[1]).resolve()
if not high.is_file() or not low.is_file():raise FileNotFoundError('Missing input blend')
if out.exists():raise FileExistsError(out)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
inputs={'high_sha256_before':sha(high),'low_sha256_before':sha(low)}
out.mkdir(parents=True)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
scene.render.bake.use_selected_to_active=True;scene.render.bake.use_pass_direct=False
scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
scene.render.bake.cage_extrusion=.15;scene.render.bake.max_ray_distance=.4
scene.render.bake.margin=8
reports=[]
for name in ('MM24_BaseTrial','MM24_YawTrial'):
    target=bpy.data.objects.get(name)
    if not target or target.type!='MESH':raise RuntimeError('Missing target '+name)
    with bpy.data.libraries.load(str(high),link=False) as (source,data):
        if name not in source.objects:raise RuntimeError('Missing source '+name)
        data.objects=[name]
    original=data.objects[0];bpy.context.collection.objects.link(original)
    image=bpy.data.images.new('MM24_TrialBake_'+name,width=2048,height=2048,alpha=False)
    for slot in target.material_slots:
        if not slot.material:raise RuntimeError('Missing target material')
        slot.material=slot.material.copy();slot.material.use_nodes=True
        nodes=slot.material.node_tree.nodes
        for node in nodes:node.select=False
        baked=nodes.new('ShaderNodeTexImage');baked.image=image;baked.select=True;nodes.active=baked
    bpy.ops.object.select_all(action='DESELECT');original.select_set(True);target.select_set(True)
    bpy.context.view_layer.objects.active=target
    bpy.ops.object.bake(type='DIFFUSE')
    image.filepath_raw=str(out/(name+'_baked.png'));image.file_format='PNG';image.save();image.pack()
    for slot in target.material_slots:
        nodes=slot.material.node_tree.nodes
        for node in nodes:node.select=False
        baked=next(n for n in nodes if n.type=='TEX_IMAGE' and n.image==image)
        baked.select=True;nodes.active=baked
    reports.append({'part':name,'baked_file':image.filepath_raw,'baked_sha256':sha(Path(image.filepath_raw))})
    bpy.data.objects.remove(original,do_unlink=True)
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='TEXTURE'
scene.render.filepath=str(out/'baked_yaw_+00.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out/'MM24_BakedTrial.blend'))
inputs.update(high_sha256_after=sha(high),low_sha256_after=sha(low))
report={'inputs':inputs,'parts':reports,'status':'trial only; inspect texture, seam, joints and gameplay separately'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if inputs['high_sha256_before']!=inputs['high_sha256_after'] or inputs['low_sha256_before']!=inputs['low_sha256_after']:
    raise RuntimeError('Input blend unexpectedly changed')
print('OV_MM24_BAKE_TRIAL',out)
