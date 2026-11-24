"""Import stage-three source exteriors with provisional placement. Socket and bay certification are separate."""
import json
import os
from pathlib import Path

import unreal as u

prepared=Path(os.environ["OV_BASE_STAGE3_PREPARED"])
manifest=json.loads((prepared/"sources.json").read_text(encoding="utf-8"))
tools=u.AssetToolsHelpers.get_asset_tools()
assets=u.EditorAssetLibrary
materials=u.MaterialEditingLibrary
root="/Game/Presentation/Base"
result={}

def import_asset(path, folder, name, factory, options=None):
    existing=assets.load_asset(folder+"/"+name) if assets.does_asset_exist(folder+"/"+name) else None
    if existing:
        return existing
    task=u.AssetImportTask()
    task.filename=str(path)
    task.destination_path=folder
    task.destination_name=name
    task.factory=factory
    task.automated=True
    task.save=False
    if options:
        task.options=options
    tools.import_asset_tasks([task])
    imported=task.get_objects()
    if not imported:
        raise RuntimeError(str(path))
    return imported[0]

def surface(name, source):
    folder=root+"/"+name
    mat=assets.load_asset(folder+"/M_Surface") if assets.does_asset_exist(folder+"/M_Surface") else None
    if mat:
        return mat
    mat=tools.create_asset("M_Surface",folder,u.Material,u.MaterialFactoryNew())
    slots=[
        ("", "Color",u.MaterialProperty.MP_BASE_COLOR,u.MaterialSamplerType.SAMPLERTYPE_COLOR,None),
        ("_normal","Normal",u.MaterialProperty.MP_NORMAL,u.MaterialSamplerType.SAMPLERTYPE_NORMAL,u.TextureCompressionSettings.TC_NORMALMAP),
        ("_metallic","Metallic",u.MaterialProperty.MP_METALLIC,u.MaterialSamplerType.SAMPLERTYPE_MASKS,u.TextureCompressionSettings.TC_MASKS),
        ("_roughness","Roughness",u.MaterialProperty.MP_ROUGHNESS,u.MaterialSamplerType.SAMPLERTYPE_MASKS,u.TextureCompressionSettings.TC_MASKS)]
    for i,(suffix,key,prop,sampler,compression) in enumerate(slots):
        texture_path=source.with_name(source.stem+suffix+".png")
        if not texture_path.exists():
            raise RuntimeError(str(texture_path))
        texture=import_asset(texture_path,folder,"T_"+key,u.TextureFactory())
        if compression is not None:
            texture.set_editor_property("srgb",False)
            texture.set_editor_property("compression_settings",compression)
        node=materials.create_material_expression(mat,u.MaterialExpressionTextureSample,-520,i*200)
        node.set_editor_property("texture",texture)
        node.set_editor_property("sampler_type",sampler)
        assert materials.connect_material_property(node,"RGB" if i<2 else "R",prop)
        assert assets.save_loaded_asset(texture,False)
    mat.set_editor_property("blend_mode",u.BlendMode.BLEND_MASKED)
    world=materials.create_material_expression(mat,u.MaterialExpressionWorldPosition,-540,850)
    mask=materials.create_material_expression(mat,u.MaterialExpressionCustom,-175,900)
    parameters=[world]
    inputs=[]
    for i,key in enumerate(("Pos","Top","Work")):
        item=u.CustomInput()
        item.set_editor_property("input_name",key)
        inputs.append(item)
        if i:
            scalar=materials.create_material_expression(mat,u.MaterialExpressionScalarParameter,-540,850+i*95)
            scalar.set_editor_property("parameter_name",key)
            scalar.set_editor_property("use_custom_primitive_data",True)
            scalar.set_editor_property("primitive_data_index",i-1)
            parameters.append(scalar)
    mask.set_editor_property("inputs",inputs)
    mask.set_editor_property("output_type",u.CustomMaterialOutputType.CMOT_FLOAT1)
    mask.set_editor_property("code","return Work < 0.5 || Pos.z <= Top ? 1.0 : 0.0;")
    for parameter,key in zip(parameters,("Pos","Top","Work")):
        assert materials.connect_material_expressions(parameter,"",mask,key)
    assert materials.connect_material_property(mask,"",u.MaterialProperty.MP_OPACITY_MASK)
    materials.recompile_material(mat)
    assert assets.save_loaded_asset(mat,False)
    return mat

for name,info in manifest.items():
    try:
        key="SM_OV_Base_"+name
        settings=u.FbxImportUI()
        settings.import_mesh=True
        settings.import_as_skeletal=False
        settings.automated_import_should_detect_type=False
        settings.mesh_type_to_import=u.FBXImportType.FBXIT_STATIC_MESH
        settings.import_materials=False
        settings.import_textures=False
        settings.static_mesh_import_data.combine_meshes=True
        settings.static_mesh_import_data.auto_generate_collision=False
        settings.static_mesh_import_data.generate_lightmap_u_vs=False
        mesh=import_asset(prepared/(key+".fbx"),root,key,u.FbxFactory(),settings)
        material=surface(name,Path(info["source"]))
        for i,_ in enumerate(mesh.get_editor_property("static_materials")):
            mesh.set_material(i,material)
        assert assets.save_loaded_asset(mesh,False)
        result[name]={"asset":mesh.get_path_name(),"triangles":mesh.get_num_triangles(0),
                      "port_status":info["port_status"],"source_sha256":info["source_sha256"]}
        u.log("BASE_STAGE3_IMPORTED "+name)
    except Exception as error:
        result[name]={"error":str(error)}
        u.log_error("BASE_STAGE3_IMPORT_FAILED "+name+" "+str(error))
    finally:
        (prepared/"import.json").write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding="utf-8")
        u.SystemLibrary.collect_garbage()
