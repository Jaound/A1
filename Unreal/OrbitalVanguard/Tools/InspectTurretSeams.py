"""Read one original FBX and render axis-aligned trial views without changing it.

Usage: blender --background --python InspectTurretSeams.py -- --source <fbx> --output <new-folder>
Cross sections are descriptive only; they do not cut or assert a mechanical seam.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Vector

parser=argparse.ArgumentParser()
parser.add_argument('--source',required=True)
parser.add_argument('--output',required=True)
args=parser.parse_args(sys.argv[sys.argv.index('--')+1:])
source=Path(args.source).resolve();out=Path(args.output).resolve()
if not source.is_file():raise FileNotFoundError(source)
if out.exists():raise FileExistsError(out)
def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
before=sha(source);out.mkdir(parents=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source))
scene=bpy.context.scene;meshes=[obj for obj in scene.objects if obj.type=='MESH']
if not meshes:raise RuntimeError('FBX contains no meshes')
corners=[obj.matrix_world@Vector(c) for obj in meshes for c in obj.bound_box]
lower=Vector(tuple(min(p[i] for p in corners) for i in range(3)))
upper=Vector(tuple(max(p[i] for p in corners) for i in range(3)))
center=(lower+upper)/2;extent=upper-lower
bins=[{'count':0,'xmin':math.inf,'xmax':-math.inf,'ymin':math.inf,'ymax':-math.inf} for _ in range(60)]
for obj in meshes:
    transform=obj.matrix_world
    for vertex in obj.data.vertices:
        p=transform@vertex.co
        i=min(59,max(0,int((p.z-lower.z)/max(extent.z,1e-9)*60)))
        b=bins[i];b['count']+=1;b['xmin']=min(b['xmin'],p.x);b['xmax']=max(b['xmax'],p.x);b['ymin']=min(b['ymin'],p.y);b['ymax']=max(b['ymax'],p.y)
for i,b in enumerate(bins):
    b['z_min']=lower.z+extent.z*i/60;b['z_max']=lower.z+extent.z*(i+1)/60
    if not b['count']:
        for key in ('xmin','xmax','ymin','ymax'):b[key]=None
camera=bpy.data.cameras.new('TrialOrthographic');camera_obj=bpy.data.objects.new('TrialOrthographic',camera)
scene.collection.objects.link(camera_obj);scene.camera=camera_obj;camera.type='ORTHO';camera.ortho_scale=max(extent)*1.22
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.light='STUDIO';scene.display.shading.color_type='MATERIAL'
scene.display.shading.show_shadows=True;scene.display.shading.show_cavity=True;scene.display.shading.background_type='WORLD'
if not scene.world:scene.world=bpy.data.worlds.new('InspectionWorld')
scene.world.color=(.025,.035,.05);scene.render.resolution_x=800;scene.render.resolution_y=800;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
for name,direction in [('from_pos_x',(1,0,0)),('from_neg_x',(-1,0,0)),('from_pos_y',(0,1,0)),('from_neg_y',(0,-1,0)),('from_pos_z',(0,0,1))]:
    camera_obj.location=center+Vector(direction)*max(extent)*3
    camera_obj.rotation_euler=(center-camera_obj.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(out/(name+'.png'));bpy.ops.render.render(write_still=True)
after=sha(source)
report={'source':str(source),'source_sha256_before':before,'source_sha256_after':after,'source_unchanged':before==after,
        'bounds_min':list(lower),'bounds_max':list(upper),'axes':'Blender imported world coordinates',
        'vertex_slices':bins,'warning':'Geometric evidence only; moving-part separation and UV/cap/pivot validation not performed.'}
(out/'analysis.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
if before!=after:raise RuntimeError('Source unexpectedly changed')
print('OV_TURRET_INSPECTION',out)
