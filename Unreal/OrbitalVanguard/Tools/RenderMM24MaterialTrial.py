"""Render a read-only original or experimental MM24 material under shared lighting."""
import hashlib
import json
from pathlib import Path
import sys
import bpy
from mathutils import Vector

args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=2 or args[1] not in ('source','trial'):raise ValueError('Pass new output directory and source or trial')
source=Path(bpy.data.filepath).resolve();out=Path(args[0]).resolve();mode=args[1]
if out.exists():raise FileExistsError(out)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);out.mkdir(parents=True)
if mode=='trial':
    for name in ('MM24_BaseTrial','MM24_YawTrial'):
        obj=bpy.data.objects.get(name)
        if not obj:raise RuntimeError('Missing '+name)
        for slot in obj.material_slots:
            mat=slot.material
            if not mat or mat.name.startswith('TrialCutCap'):continue
            nodes=mat.node_tree.nodes;links=mat.node_tree.links
            bsdf=next(n for n in nodes if n.type=='BSDF_PRINCIPLED')
            def image_node(prefix):
                return next(n for n in nodes if n.type=='TEX_IMAGE' and n.image and n.image.name.startswith(prefix))
            links.new(image_node('MM24_TrialBake_').outputs['Color'],bsdf.inputs['Base Color'])
            links.new(image_node('Trial_Roughness_').outputs['Color'],bsdf.inputs['Roughness'])
            links.new(image_node('Trial_Metallic_').outputs['Color'],bsdf.inputs['Metallic'])
            normal=nodes.new('ShaderNodeNormalMap')
            links.new(image_node('Trial_NORMAL_').outputs['Color'],normal.inputs['Color'])
            links.new(normal.outputs['Normal'],bsdf.inputs['Normal'])
scene=bpy.context.scene;scene.render.engine='CYCLES';scene.cycles.samples=8
scene.cycles.use_denoising=True;scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.world.use_nodes=True;scene.world.node_tree.nodes.get('Background').inputs['Color'].default_value=(.18,.18,.18,1)
scene.world.node_tree.nodes.get('Background').inputs['Strength'].default_value=.6
light=bpy.data.lights.new('TrialComparisonLight','AREA');light.energy=600;light.shape='DISK';light.size=5
obj=bpy.data.objects.new('TrialComparisonLight',light);scene.collection.objects.link(obj);obj.location=(3,-4,6)
obj.rotation_euler=(Vector((0,0,.3))-obj.location).to_track_quat('-Z','Y').to_euler()
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(out/'material.png');bpy.ops.render.render(write_still=True)
after=sha(source);report={'mode':mode,'input':str(source),'sha256_before':before,'sha256_after':after,
 'unchanged':before==after,'scope':'Blender trial lighting only; no UE, animation, performance or art approval'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('Input blend changed')
print('OV_MM24_MATERIAL_VIEW',out)
