"""Bake original colour onto a separate MM24 low-poly trial; never modifies inputs.

This is a visual experiment, not gameplay or art approval. The pitch joint remains
fused. Run in Blender with the low-poly .blend open and pass high-poly .blend and
a new output directory after --.
"""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy

args=sys.argv[sys.argv.index('--')+1:]
smart_uv='--smart-uv' in args
args=[a for a in args if a!='--smart-uv']
align_target='--align-target' in args
args=[a for a in args if a!='--align-target']
fill_empty='--fill-empty-trial-material' in args
args=[a for a in args if a!='--fill-empty-trial-material']
parts=next((a.split('=',1)[1].split(',') for a in args if a.startswith('--parts=')),['BaseTrial','YawTrial'])
args=[a for a in args if not a.startswith('--parts=')]
prefix=next((a.split('=',1)[1] for a in args if a.startswith('--prefix=')),'MM24')
args=[a for a in args if not a.startswith('--prefix=')]
if not prefix.replace('_','').isalnum() or not parts or any(not p.replace('_','').isalnum() for p in parts):raise ValueError('Invalid part prefix or names')
if len(args) not in (2,4):raise ValueError('Pass high-poly .blend, new output directory, optionally cage extrusion and ray distance')
high=Path(args[0]).resolve();low=Path(bpy.data.filepath).resolve();out=Path(args[1]).resolve()
extrusion=float(args[2]) if len(args)==4 else .15;ray_distance=float(args[3]) if len(args)==4 else .4
if extrusion<=0 or ray_distance<=0:raise ValueError('Bake distances must be positive')
if not high.is_file() or not low.is_file():raise FileNotFoundError('Missing input blend')
if out.exists():raise FileExistsError(out)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
inputs={'high_sha256_before':sha(high),'low_sha256_before':sha(low)}
out.mkdir(parents=True)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
scene.render.bake.use_selected_to_active=True;scene.render.bake.use_pass_direct=False
scene.render.bake.use_pass_indirect=False;scene.render.bake.use_pass_color=True
scene.render.bake.cage_extrusion=extrusion;scene.render.bake.max_ray_distance=ray_distance
scene.render.bake.margin=8
reports=[]
for name in (prefix+'_'+part for part in parts):
    target=bpy.data.objects.get(name)
    if not target or target.type!='MESH':raise RuntimeError('Missing target '+name)
    with bpy.data.libraries.load(str(high),link=False) as (source,data):
        if name not in source.objects:raise RuntimeError('Missing source '+name)
        data.objects=[name]
    original=data.objects[0];bpy.context.collection.objects.link(original)
    if align_target:
        bpy.context.view_layer.update();original.matrix_world=target.matrix_world.copy();bpy.context.view_layer.update()
    if smart_uv:
        layer=target.data.uv_layers.new(name='TrialBakeUV');target.data.uv_layers.active=layer;layer.active_render=True
        bpy.ops.object.select_all(action='DESELECT');target.select_set(True);bpy.context.view_layer.objects.active=target
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(angle_limit=math.radians(66),island_margin=.002)
        bpy.ops.object.mode_set(mode='OBJECT')
    image=bpy.data.images.new(prefix+'_TrialBake_'+name,width=2048,height=2048,alpha=False)
    cap_image=bpy.data.images.new(prefix+'_TrialBakeCap_'+name,width=256,height=256,alpha=False)
    for slot in target.material_slots:
        if not slot.material:
            if not fill_empty:raise RuntimeError('Missing target material')
            slot.material=bpy.data.materials.new('TrialBooleanSurface');slot.material.diffuse_color=(.22,.23,.25,1)
        slot.material=slot.material.copy();slot.material.use_nodes=True
        nodes=slot.material.node_tree.nodes
        for node in nodes:node.select=False
        baked=nodes.new('ShaderNodeTexImage');baked.image=cap_image if slot.material.name.startswith(('TrialCutCap','TrialPitchCutCap','TrialBooleanSurface')) else image
        baked.select=True;nodes.active=baked
    bpy.ops.object.select_all(action='DESELECT');original.select_set(True);target.select_set(True)
    bpy.context.view_layer.objects.active=target
    bpy.ops.object.bake(type='DIFFUSE')
    image.filepath_raw=str(out/(name+'_baked.png'));image.file_format='PNG';image.save();image.pack()
    cap_image.filepath_raw=str(out/(name+'_cap_baked.png'));cap_image.file_format='PNG';cap_image.save();cap_image.pack()
    for slot in target.material_slots:
        nodes=slot.material.node_tree.nodes
        for node in nodes:node.select=False
        baked=next(n for n in nodes if n.type=='TEX_IMAGE' and n.image in (image,cap_image))
        baked.select=True;nodes.active=baked
    reports.append({'part':name,'baked_file':image.filepath_raw,'baked_sha256':sha(Path(image.filepath_raw)),
                    'cap_file':cap_image.filepath_raw,'cap_sha256':sha(Path(cap_image.filepath_raw))})
    bpy.data.objects.remove(original,do_unlink=True)
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='TEXTURE'
scene.render.filepath=str(out/'baked_yaw_+00.png');bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out/(prefix+'_BakedTrial.blend')))
inputs.update(high_sha256_after=sha(high),low_sha256_after=sha(low))
report={'inputs':inputs,'parts':reports,'cage_extrusion':extrusion,'max_ray_distance':ray_distance,'smart_uv':smart_uv,'align_target':align_target,'fill_empty_trial_material':fill_empty,
        'status':'trial only; inspect texture, seam, joints and gameplay separately'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if inputs['high_sha256_before']!=inputs['high_sha256_after'] or inputs['low_sha256_before']!=inputs['low_sha256_after']:
    raise RuntimeError('Input blend unexpectedly changed')
print('OV_MM24_BAKE_TRIAL',out)
