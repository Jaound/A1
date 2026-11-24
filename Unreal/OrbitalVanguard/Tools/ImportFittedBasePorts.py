"""UE editor-only import. Preserve originals' materials, LODs and existing socket objects."""
import json
import os
from pathlib import Path
import unreal as u

prepared=Path(os.environ["OV_BASE_STAGE3_PREPARED"])/"FittedPorts"
records=json.loads((prepared/"ports.json").read_text())
assert len(records)==13
folder="/Game/Presentation/Base"
library=u.EditorAssetLibrary
steel=library.load_asset(folder+"/M_PortSteel")
assert steel
for name,data in records.items():
    original=library.load_asset(folder+"/SM_OV_Base_"+name)
    assert original and original.get_num_triangles(0)>1000
    stem="SM_OV_Ports_"+name
    options=u.FbxImportUI()
    options.import_mesh=True
    options.import_as_skeletal=False
    options.automated_import_should_detect_type=False
    options.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials=False
    options.import_textures=False
    options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=False
    options.static_mesh_import_data.combine_meshes=True
    task=u.AssetImportTask()
    task.filename=str(prepared/(stem+".fbx"))
    task.destination_path=folder
    task.destination_name=stem
    task.factory=u.FbxFactory()
    task.options=options
    task.replace_existing=True
    task.automated=True
    task.save=False
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sleeve=library.load_asset(folder+"/"+stem)
    assert sleeve and sleeve.get_num_triangles(0)>=100
    sleeve.set_material(0,steel)
    assert library.save_loaded_asset(sleeve,False)
    changed=False
    for key,info in data.items():
        if key=="bay_trials": continue
        point=u.Vector(*info["face_cm"])
        rotation=u.Rotator(yaw={"XP":0,"XN":180,"YP":90,"YN":-90}[key])
        socket=original.find_socket(key)
        if socket and (socket.get_editor_property("relative_location")-point).length()<.01 and abs(u.MathLibrary.get_forward_vector(socket.get_editor_property("relative_rotation")).dot(u.MathLibrary.get_forward_vector(rotation))-1)<.000001 and abs(socket.get_editor_property("relative_rotation").roll)<.01:
            continue
        changed=True
        if not socket:
            socket=u.new_object(u.StaticMeshSocket,outer=original)
            socket.set_editor_property("socket_name",key)
            original.add_socket(socket)
        socket.set_editor_property("relative_location",point)
        socket.set_editor_property("relative_rotation",rotation)
        assert (original.find_socket(key).get_editor_property("relative_location")-point).length()<.01
    if name.startswith(("Yard","Dock")) or name=="LogisticsTerminal":
        body=original.get_editor_property("body_setup")
        if body.get_editor_property("collision_trace_flag")!=u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
            body.set_editor_property("collision_trace_flag",u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
            changed=True
    if changed: assert library.save_loaded_asset(original,False)
    u.log("BASE_PORTS_SAVED "+name)
    u.SystemLibrary.collect_garbage()
