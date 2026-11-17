"""Create a separate low-poly base/yaw trial from the high-poly MM24 trial.

The source blend is never saved. Simplification and UV appearance require visual
approval; the yaw part still includes the unsplit pitch structure.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
import bmesh

parser=argparse.ArgumentParser();parser.add_argument('--output',required=True)
parser.add_argument('--base-faces',type=int,default=10000);parser.add_argument('--yaw-faces',type=int,default=20000)
parser.add_argument('--weighted-normals',action='store_true')
parser.add_argument('--transfer-uv',action='store_true')
parser.add_argument('--protect-uv-seams',action='store_true')
parser.add_argument('--uv-seam-strength',type=float,default=1000.0)
parser.add_argument('--uv-seam-background-weight',type=float,default=0.0)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
source=Path(bpy.data.filepath).resolve();out=Path(args.output).resolve()
if min(args.base_faces,args.yaw_faces)<=0 or not 0<=args.uv_seam_strength<=1000 or not 0<=args.uv_seam_background_weight<=1:raise ValueError('Invalid target or UV seam settings')
if not source.is_file():raise FileNotFoundError(source)
if out.exists():raise FileExistsError(out)
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);out.mkdir(parents=True);scene=bpy.context.scene;results={};textures=[]
texture_dir=out/'Textures';texture_dir.mkdir()
for image in bpy.data.images:
    if not image.packed_file:continue
    suffix=Path(image.filepath).suffix.lower() or '.png'
    name=image.name if image.name.lower().endswith(suffix) else image.name+suffix
    target=texture_dir/name;target.write_bytes(bytes(image.packed_file.data));image.filepath=str(target)
    textures.append({'name':name,'sha256':sha(target),'bytes':target.stat().st_size})
if len(textures)<4:raise RuntimeError('Expected four packed source textures')
for name,target in [('MM24_BaseTrial',args.base_faces),('MM24_YawTrial',args.yaw_faces)]:
    obj=bpy.data.objects.get(name)
    if not obj or obj.type!='MESH':raise RuntimeError('Missing '+name)
    before_faces=len(obj.data.polygons);before_uv=[u.name for u in obj.data.uv_layers]
    source_uv=None
    if args.transfer_uv:
        source_uv=obj.copy();source_uv.data=obj.data.copy()
        bpy.context.collection.objects.link(source_uv)
        source_uv.hide_render=True
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    modifier=obj.modifiers.new('TrialSimplification','DECIMATE');modifier.decimate_type='COLLAPSE'
    protected=0
    if args.protect_uv_seams:
        bm=bmesh.new();bm.from_mesh(obj.data);bm.verts.index_update()
        uv=bm.loops.layers.uv.active
        if not uv:raise RuntimeError('UV seam protection requires an active UV layer')
        indices=set()
        for edge in bm.edges:
            loops=edge.link_loops
            seam=len(loops)!=2
            if not seam:
                a,b=loops
                def at(loop,vertex):
                    return loop[uv].uv if loop.vert==vertex else loop.link_loop_next[uv].uv
                seam=any((at(a,v)-at(b,v)).length_squared>1e-10 for v in edge.verts)
            if seam:
                indices.update(v.index for v in edge.verts)
        bm.free();protected=len(indices)
        group=obj.vertex_groups.new(name='TrialUVBoundary')
        if args.uv_seam_background_weight:group.add(list(range(len(obj.data.vertices))),args.uv_seam_background_weight,'REPLACE')
        if indices:group.add(sorted(indices),1.0,'REPLACE')
        modifier.vertex_group=group.name;modifier.vertex_group_factor=args.uv_seam_strength
    modifier.ratio=min(1.0,target/max(1,before_faces));modifier.use_collapse_triangulate=True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    if source_uv:
        transfer=obj.modifiers.new('TrialSourceUV','DATA_TRANSFER')
        transfer.object=source_uv;transfer.use_loop_data=True
        transfer.data_types_loops={'UV'};transfer.loop_mapping='POLYINTERP_NEAREST'
        bpy.ops.object.modifier_apply(modifier=transfer.name)
        bpy.data.objects.remove(source_uv,do_unlink=True)
    sharp_count=0
    if args.weighted_normals:
        bm=bmesh.new();bm.from_mesh(obj.data);bm.normal_update()
        for edge in bm.edges:
            if edge.is_manifold and edge.calc_face_angle(0)>math.radians(28):edge.smooth=False;sharp_count+=1
        bm.to_mesh(obj.data);bm.free()
        normal=obj.modifiers.new('TrialWeightedNormals','WEIGHTED_NORMAL');normal.keep_sharp=True
        bpy.ops.object.modifier_apply(modifier=normal.name)
    results[name]={'source_faces':before_faces,'target_faces':target,'output_faces':len(obj.data.polygons),
                   'source_uv_layers':before_uv,'output_uv_layers':[u.name for u in obj.data.uv_layers],
                   'weighted_normals':args.weighted_normals,'uv_transfer':args.transfer_uv,
                   'protected_uv_seam_vertices':protected,'uv_seam_strength':args.uv_seam_strength if args.protect_uv_seams else None,
                   'uv_seam_background_weight':args.uv_seam_background_weight if args.protect_uv_seams else None,
                   'sharp_edges_by_28deg':sharp_count}
    fbx=out/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH'},apply_unit_scale=True,
                             global_scale=1.0,axis_forward='-Y',axis_up='Z',add_leaf_bones=False,bake_anim=False,
                             path_mode='COPY',embed_textures=False)
    results[name]['fbx_sha256']=sha(fbx)
base=bpy.data.objects['MM24_BaseTrial'];yaw=bpy.data.objects['MM24_YawTrial']
for angle in (0,45,-45):
    yaw.rotation_euler.z=angle*3.141592653589793/180
    scene.render.filepath=str(out/('light_yaw_%+03d.png'%angle));bpy.ops.render.render(write_still=True)
yaw.rotation_euler.z=0;bpy.ops.wm.save_as_mainfile(filepath=str(out/'MM24_LightTrial.blend'))
after=sha(source)
report={'source_blend':str(source),'source_sha256_before':before,'source_sha256_after':after,'source_unchanged':before==after,
        'parts':results,'extracted_packed_textures':textures,'status':'simplified trial only; UV appearance, cap contact, pitch separation and UE gameplay remain unapproved'}
(out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if before!=after:raise RuntimeError('Source trial unexpectedly changed')
print('OV_MM24_LIGHT_TRIAL',out)
