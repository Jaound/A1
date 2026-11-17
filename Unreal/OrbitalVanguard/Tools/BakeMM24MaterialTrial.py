"""Bake trial tangent normals and scalar material maps onto the color trial UV.

Inputs are read only; output maps are experiments until UE material and visual
comparisons are validated. Tangent normal baking is not proof that the original
normal-map detail was reproduced.
"""
import hashlib
import json
from pathlib import Path
import sys
import bpy

args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2:raise ValueError('Pass high-poly blend and a new output directory')
high=Path(args[0]).resolve();low=Path(bpy.data.filepath).resolve();out=Path(args[1]).resolve()
if not high.is_file() or not low.is_file():raise FileNotFoundError('Missing input blend')
if out.exists():raise FileExistsError(out)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
inputs={'high_before':sha(high),'low_before':sha(low)};out.mkdir(parents=True)
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=1
scene.render.bake.use_selected_to_active=True;scene.render.bake.cage_extrusion=.02
scene.render.bake.max_ray_distance=.06;scene.render.bake.margin=8
results=[]
for name in ('MM24_BaseTrial','MM24_YawTrial'):
    target=bpy.data.objects.get(name)
    if not target or not target.data.uv_layers.get('TrialBakeUV'):raise RuntimeError('Missing target trial UV '+name)
    target.data.uv_layers.active=target.data.uv_layers['TrialBakeUV']
    with bpy.data.libraries.load(str(high),link=False) as (source,data):
        if name not in source.objects:raise RuntimeError('Missing source '+name)
        data.objects=[name]
    original=data.objects[0];bpy.context.collection.objects.link(original)
    for slot in target.material_slots:
        if not slot.material:raise RuntimeError('Missing target material')
        slot.material=slot.material.copy()
    for slot in original.material_slots:
        if slot.material:slot.material=slot.material.copy()
    for channel in ('NORMAL','Roughness','Metallic'):
        image=bpy.data.images.new('Trial_'+channel+'_'+name,width=2048,height=2048,alpha=False)
        image.colorspace_settings.name='Non-Color'
        cap=bpy.data.images.new('Trial_'+channel+'_Cap_'+name,width=256,height=256,alpha=False)
        cap.colorspace_settings.name='Non-Color'
        for slot in target.material_slots:
            nodes=slot.material.node_tree.nodes
            for node in nodes:node.select=False
            node=nodes.new('ShaderNodeTexImage');node.image=cap if slot.material.name.startswith('TrialCutCap') else image
            node.select=True;nodes.active=node
        if channel!='NORMAL':
            for slot in original.material_slots:
                if not slot.material:continue
                nodes=slot.material.node_tree.nodes;links=slot.material.node_tree.links
                shader=next((n for n in nodes if n.type=='BSDF_PRINCIPLED'),None)
                output=next((n for n in nodes if n.type=='OUTPUT_MATERIAL'),None)
                if not shader or not output:raise RuntimeError('Missing source shader')
                emission=nodes.new('ShaderNodeEmission')
                value=shader.inputs[channel]
                if value.is_linked:links.new(value.links[0].from_socket,emission.inputs['Color'])
                else:emission.inputs['Color'].default_value=(value.default_value,)*3+(1,)
                links.new(emission.outputs['Emission'],output.inputs['Surface'])
        bpy.ops.object.select_all(action='DESELECT');original.select_set(True);target.select_set(True)
        bpy.context.view_layer.objects.active=target
        bpy.ops.object.bake(type='NORMAL' if channel=='NORMAL' else 'EMIT')
        for img,suffix in ((image,''),(cap,'_cap')):
            path=out/(name+'_'+channel+suffix+'.png');img.filepath_raw=str(path);img.file_format='PNG';img.save();img.pack()
            results.append({'part':name,'channel':channel,'cap':bool(suffix),'path':str(path),'sha256':sha(path)})
    bpy.data.objects.remove(original,do_unlink=True)
bpy.ops.wm.save_as_mainfile(filepath=str(out/'MM24_MaterialTrial.blend'))
inputs.update(high_after=sha(high),low_after=sha(low))
report={'inputs':inputs,'maps':results,'status':'trial only; material appearance and original normal detail not yet verified'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if inputs['high_before']!=inputs['high_after'] or inputs['low_before']!=inputs['low_after']:raise RuntimeError('Input changed')
print('OV_MM24_MATERIAL_TRIAL',out)
