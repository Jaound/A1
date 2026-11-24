"""Blender source preparation for unverified stage-three exterior meshes."""
import hashlib
import json
import os
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

source=Path(os.environ.get("OV_BASE_ASSET_ROOT", Path(__file__).resolve().parents[4] / "OrbitalVanguardAssets"))/"Source"/"Base"
output=Path(os.environ["OV_BASE_STAGE3_PREPARED"])
output.mkdir(parents=True,exist_ok=True)
entries=[
    ("YardMedium","중형 조선소",(6,4,4)),("YardLarge","대형 조선소",(9,6,5)),
    ("YardSuperheavy","초대형 조선소",(12,8,7)),("DockMedium","중형 접안 모듈",(5,4,3)),
    ("DockLarge","대형 접안 모듈",(7,5,4)),("DockSuperheavy","초대형 접안 모듈",(10,7,6)),
    ("LogisticsTerminal","물류 터미널 모듈",(3,2,2)),("ResearchLab","연구소 모듈",(2,2,2)),
    ("HeroAcademy","영웅 아카데미 모듈",(2,2,2)),("CrewSupport","인원 지원소",(2,2,2)),
    ("DefensePlatform","방어 플랫폼 모듈",(2,2,2)),("RadarArray","레이더 모듈",(2,2,3)),
    ("ShieldBastion","기지 보호 모듈",(3,3,3))]
manifest_path=output/"sources.json"
manifest=json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
for name,folder,dims in entries:
    path=next((source/folder).rglob("*.fbx"))
    with path.open("rb") as stream:
        if stream.read(18)!=b"Kaydara FBX Binary":
            print("BASE_STAGE3_UNREADABLE",name,path,flush=True)
            continue
        stream.seek(0)
        digest=hashlib.file_digest(stream,"sha256").hexdigest()
    mesh_path=output/("SM_OV_Base_"+name+".fbx")
    if mesh_path.exists() and manifest.get(name,{}).get("source_sha256")==digest:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path),use_image_search=False)
    meshes=[obj for obj in bpy.context.scene.objects if obj.type=="MESH"]
    if not meshes:
        raise RuntimeError(path)
    bpy.ops.object.select_all(action="DESELECT")
    for mesh in meshes:
        mesh.select_set(True)
    bpy.context.view_layer.objects.active=meshes[0]
    if len(meshes)>1:
        bpy.ops.object.join()
    mesh=bpy.context.view_layer.objects.active
    mesh.data.transform(mesh.matrix_world)
    mesh.matrix_world=Matrix.Identity(4)
    corners=list(map(Vector,mesh.bound_box))
    lower=Vector(tuple(min(p[axis] for p in corners) for axis in range(3)))
    upper=Vector(tuple(max(p[axis] for p in corners) for axis in range(3)))
    bounds=upper-lower
    scale=min(0.94*25*dims[axis]/bounds[axis] for axis in range(3) if bounds[axis]>0.001)
    center=(lower+upper)*0.5
    target=Vector((12.5*(dims[0]-1),-12.5*(dims[1]-1),12.5*(dims[2]-1)))
    mesh.data.transform(Matrix.Translation(target) @ Matrix.Scale(scale,4) @ Matrix.Translation(-center))
    mesh.name=mesh_path.stem
    bpy.context.scene.unit_settings.system="METRIC"
    bpy.ops.export_scene.fbx(filepath=str(mesh_path),use_selection=True,object_types={"MESH"},
        axis_forward="-Y",axis_up="Z",bake_anim=False,embed_textures=False,path_mode="STRIP")
    manifest[name]={"source_sha256":digest,"source":str(path),"size_m":list(bounds*scale),
        "triangles":len(mesh.data.polygons),"port_status":"unmeasured_exterior"}
    manifest_path.write_text(json.dumps(manifest,indent=2,ensure_ascii=False),encoding="utf-8")
    print("BASE_STAGE3_PREPARED",name,flush=True)
