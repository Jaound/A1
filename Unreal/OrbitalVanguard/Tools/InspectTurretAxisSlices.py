"""Inspect trial mesh extents along a chosen axis without changing the input.

Slices are evidence only: they do not establish a pitch joint or authorize an
automatic cut through a fused source surface.
"""
import hashlib
import json
import math
from pathlib import Path
import sys
import bpy
from mathutils import Vector

args=sys.argv[sys.argv.index('--')+1:]
if len(args)!=3 or args[1] not in ('x','y','z'):raise ValueError('Pass mesh name, axis and new report path')
name,axis=args[:2];out=Path(args[2]).resolve();source=Path(bpy.data.filepath).resolve()
if out.exists():raise FileExistsError(out)
obj=bpy.data.objects.get(name)
if not obj or obj.type!='MESH':raise RuntimeError('Missing mesh '+name)
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
before=sha(source);axis_index='xyz'.index(axis);other=[i for i in range(3) if i!=axis_index]
corners=[obj.matrix_world@Vector(c) for c in obj.bound_box]
lo=min(p[axis_index] for p in corners);hi=max(p[axis_index] for p in corners)
bins=[{'count':0,'min':[math.inf,math.inf],'max':[-math.inf,-math.inf]} for _ in range(100)]
for vertex in obj.data.vertices:
    p=obj.matrix_world@vertex.co;i=min(99,max(0,int((p[axis_index]-lo)/max(hi-lo,1e-9)*100)));b=bins[i];b['count']+=1
    for j,k in enumerate(other):b['min'][j]=min(b['min'][j],p[k]);b['max'][j]=max(b['max'][j],p[k])
for i,b in enumerate(bins):
    b['axis_min']=lo+(hi-lo)*i/100;b['axis_max']=lo+(hi-lo)*(i+1)/100
    if not b['count']:b['min']=b['max']=None
after=sha(source);report={'source':str(source),'source_before':before,'source_after':after,'mesh':name,'axis':axis,
 'other_axes':['xyz'[i] for i in other],'slices':bins,'scope':'read-only geometry evidence; no pitch split or art approval'}
out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
if before!=after:raise RuntimeError('Source unexpectedly changed')
print('OV_TURRET_AXIS_SLICES',out)
