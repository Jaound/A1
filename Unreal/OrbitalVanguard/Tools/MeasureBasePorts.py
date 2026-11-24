"""Fit hollow collars to the prepared exteriors. Run with Blender in background."""
import json
import os
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

root = Path(os.environ["OV_BASE_STAGE3_PREPARED"])
out = root / "FittedPorts"
out.mkdir(exist_ok=True)
# Logical cell counts match the saved module definitions. World positions below
# are in Blender metres (+Y points towards UE -Y).
dims = dict(YardMedium=(6,4,4), YardLarge=(9,6,5), YardSuperheavy=(12,8,7),
            DockMedium=(5,4,3), DockLarge=(7,5,4), DockSuperheavy=(10,7,6),
            LogisticsTerminal=(3,2,2), ResearchLab=(2,2,2), HeroAcademy=(2,2,2),
            CrewSupport=(2,2,2), DefensePlatform=(2,2,2), RadarArray=(2,2,3),
            ShieldBastion=(3,3,3))
ring = [(-.55,-1),(.55,-1),(1,-.55),(1,.55),(.55,1),(-.55,1),(-1,.55),(-1,-.55)]
up = Vector((0,0,1))
report = {}

for name,(nx,ny,nz) in dims.items():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(root / ("SM_OV_Base_"+name+".fbx")),use_image_search=False)
    obj, = [o for o in bpy.context.scene.objects if o.type=="MESH"]
    obj.data.transform(obj.matrix_world)
    obj.matrix_world.identity()
    tree = BVHTree.FromObject(obj,bpy.context.evaluated_depsgraph_get())
    # Transform the grid port coordinates only once. The collar can bend between
    # a supported patch of hull and its logical port without shifting the body.
    ports = {"XN":((-.5,0,0),(-1,0,0))}
    if name.startswith("Yard"):
        ports={"YN":((nx-1,.5,0),(0,1,0)), "YP":((nx-1,-ny+.5,0),(0,-1,0))}
    elif name.startswith("Dock") or name=="LogisticsTerminal":
        ports={"XN":((-.5,-1,0),(-1,0,0)), "XP":((nx-.5,-1,0),(1,0,0))}
    verts, faces, measurements = [], [], {}
    rim=json.loads((out/"rim.json").read_text())  # MeasureCorridorRim.py
    depths=[gap for samples in rim.values() for gap in samples]
    if any(gap is None for gap in depths): raise ValueError("corridor lip has an unsupported sample")
    seal_depth=(max(depths)+2)/100
    for socket,(cell,direction) in ports.items():
        end, normal = Vector(cell)*25, Vector(direction)
        tangent = up.cross(normal)
        def scan(dx,dz):
            pos=end+tangent*dx+up*dz
            hit=tree.ray_cast(pos+normal*3,-normal,320)[0]
            return None if hit is None else (hit-end).dot(normal)
        patches=[]
        for dx in range(-min(120,nx*25),min(120,nx*25)+1,2):
            for dz in range(-10, nz*25,2):
                depths=[scan(dx+x*2.4,dz+z*2.9) for x,z in ring+[(0,0)]+[(x*2.1/2.4,z*2.6/2.9) for x,z in ring]]
                if None in depths or max(depths)>.1:
                    continue
                patches.append((dx*dx+dz*dz+max(depths)**2+30*(max(depths)-min(depths))**2,dx,dz,depths))
        if not patches:
            raise ValueError((name,socket,"no supported port footprint"))
        _,dx,dz,depths=min(patches)
        wall=end+tangent*dx+up*dz
        shoulder=wall+normal*(max(depths)+.22)
        first=len(verts)
        for section,(center,w,h) in enumerate([(wall+normal*(min(depths)-.18),2.4,2.9),(shoulder,2.4,2.9),
                            (end-normal*.35,2.18,2.68),(end,2.18,2.68),(end,1.90,2.38),
                            (end-normal*.35,1.90,2.38),(shoulder,2.10,2.60),(wall+normal*(min(depths)-.18),2.10,2.60)]):
            for x,z in ring:
                point=center+tangent*x*w+up*z*h
                if section in (0,7):
                    depth=scan(dx+x*w,dz+z*h)
                    if depth is None: raise ValueError((name,socket,"unsupported sleeve root"))
                    point=wall+tangent*x*w+up*z*h+normal*(depth-.18)
                verts.append(point)
        for i in range(8):
            for j in range(8):
                a,b=first+i*8,first+((i+1)%8)*8
                k=(j+1)%8
                faces.append((a+j,a+k,b+k,b+j))
        # Recessed sleeve insert bridges the measured crown groove. The mating
        # datum and outer rim stay at zero; only this hidden insert enters the bore.
        first=len(verts)
        for distance,w,h in [(-.08,2.17,2.67),(seal_depth,2.17,2.67),
                             (seal_depth,1.91,2.39),(-.08,1.91,2.39)]:
            verts.extend(end+normal*distance+tangent*x*w+up*z*h for x,z in ring)
        for i in range(4):
            for j in range(8):
                a,b=first+i*8,first+((i+1)%4)*8
                k=(j+1)%8
                faces.append((a+j,a+k,b+k,b+j))
        def ue(v):
            return [round(v.x*100,2),round(-v.y*100,2),round(v.z*100,2)]
        measurements[socket]=dict(face_cm=ue(end),shell_cm=ue(wall+normal*sum(depths)/len(depths)),
                                  shell_depth_span_cm=round(100*(max(depths)-min(depths)),2))
    if name.startswith(("Dock","Yard")):
        yard=name.startswith("Yard")
        direction=Vector((1,0,0) if yard else (0,1,0))
        across=Vector((0,1,0) if yard else (1,0,0))
        corners=[Vector(v) for v in obj.bound_box]
        front=min(v.dot(direction) for v in corners)
        back=max(v.dot(direction) for v in corners)
        lo=min(v.dot(across) for v in corners)
        hi=max(v.dot(across) for v in corners)
        zlo=min(v.z for v in corners)
        zhi=max(v.z for v in corners)
        paths=[]
        for lateral in range(int(lo)+4,int(hi)-3,3):
            for z in range(int(zlo)+3,int(zhi)-2,3):
                start=(front-20)*direction+lateral*across+z*up
                contacts=[tree.ray_cast(start+across*y+up*h,direction,back-front+20)[3]
                          for y in (-3,0,3) for h in (-2,0,2)]
                clear=min([d for d in contacts if d is not None]+[back-front+20])
                if clear < 37+(back-front)*.20: continue
                travel=min(clear-10,20+(back-front)*.52)
                end=start+direction*travel
                floor=tree.ray_cast(end,-up,25)[0]
                if floor is None or end.z-floor.z<2.3: continue
                sides=[tree.ray_cast(end,sign*across,(hi-lo)*.65)[0] for sign in (-1,1)]
                if not any(v is not None for v in sides): continue
                score=(travel-20)/(back-front)-.015*(end.z-floor.z)-.002*abs(lateral-(lo+hi)*.5)
                paths.append((score,dict(start=ue(start),end=ue(end),floor=ue(floor))))
        paths.sort(key=lambda item:-item[0])
        # Keep alternatives where the full swept box encounters a projection.
        measurements["bay_trials"]=[path for _,path in paths[:80]]
    mesh=bpy.data.meshes.new("PortSleeves")
    mesh.from_pydata(verts,[],faces)
    mesh.update()
    part=bpy.data.objects.new("SM_OV_Ports_"+name,mesh)
    bpy.context.collection.objects.link(part)
    bpy.ops.object.select_all(action='DESELECT')
    part.select_set(True)
    bpy.context.view_layer.objects.active=part
    bpy.context.scene.unit_settings.system='METRIC'
    bpy.ops.export_scene.fbx(filepath=str(out/(part.name+".fbx")),use_selection=True,object_types={'MESH'},
                             axis_forward='-Y',axis_up='Z',bake_anim=False)
    report[name]=measurements
    (out/"ports.json").write_text(json.dumps(report,indent=2),encoding='utf8')
    print("FITTED_PORTS",name,flush=True)
