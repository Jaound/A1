"""Measure the original corridor cap against the sleeve's mating annulus at both ends."""
import json
import os
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

assets=Path(os.environ["OV_BASE_ASSET_ROOT"])
prepared=assets/"Prepared"/"BaseTrial1"
spec=json.loads((prepared/"ports.json").read_text(encoding="utf8"))["CorridorStraight_1x1x1"]
source=next((assets/"Source"/"Base"/"일자 통로 모듈").rglob("*.fbx"))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source),use_image_search=False)
obj,=[o for o in bpy.context.scene.objects if o.type=="MESH"]
from mathutils import Matrix
obj.data.transform(Matrix.Scale(1.812,4)@Matrix.Translation(Vector((0,0,-2.005)))@obj.matrix_world)
obj.matrix_world=Matrix.Identity(4)
tree=BVHTree.FromObject(obj,bpy.context.evaluated_depsgraph_get())
ring=[(-.55,-1),(.55,-1),(1,-.55),(1,.55),(.55,1),(-.55,1),(-1,.55),(-1,-.55)]
report={}
for key,xyz in spec["ports"].items():
    p=Vector((xyz[0],-xyz[1],xyz[2]))*.01
    n=Vector((1 if key=="XP" else -1,0,0))
    gaps=[]
    for i,(x,z) in enumerate(ring):
        xx,zz=ring[(i+1)%8]
        for f in (.0,.25,.5,.75):
            q=p+Vector((0,(x*(1-f)+xx*f)*2.04,(z*(1-f)+zz*f)*2.53))
            hit,_,_,_=tree.ray_cast(q+n*1.0,-n,4)
            gaps.append(None if hit is None else round(100*(q-hit).dot(n),2))
    report[key]=gaps
out=Path(os.environ["OV_BASE_STAGE3_PREPARED"])/"FittedPorts"/"rim.json"
out.write_text(json.dumps(report,indent=2))
print("RIM_PROFILE",report,flush=True)
