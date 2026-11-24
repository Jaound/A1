"""준비한 네 모듈을 UE 메시·재질·명명된 소켓으로 반입한다. 원본과 기존 자산을 보호한다."""
from pathlib import Path
import json
import unreal

root=Path.home()/'Documents/OrbitalVanguardAssets/Prepared/BaseTrial1'
report=json.loads((root/'ports.json').read_text())
assert len(report)==4
tools=unreal.AssetToolsHelpers.get_asset_tools()
library=unreal.EditorAssetLibrary
matlib=unreal.MaterialEditingLibrary
for module,data in report.items():
    dest='/Game/Presentation/Base'
    def import_asset(path,name,factory,options=None):
        asset_path=dest+'/'+name
        if library.does_asset_exist(asset_path):
            raise RuntimeError('기존 에셋 확인 필요 '+asset_path)
        task=unreal.AssetImportTask()
        task.filename=str(path)
        task.destination_path=dest
        task.destination_name=name
        task.automated=True
        task.save=False
        task.factory=factory
        if options: task.options=options
        tools.import_asset_tasks([task])
        result=library.load_asset(asset_path)
        if result is None: raise RuntimeError(str(path))
        return result
    name='SM_OV_Base_'+module
    options=unreal.FbxImportUI()
    options.import_mesh=True
    options.import_as_skeletal=False
    options.automated_import_should_detect_type=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.import_materials=False
    options.import_textures=False
    settings=options.static_mesh_import_data
    settings.combine_meshes=True
    settings.auto_generate_collision=False
    settings.generate_lightmap_u_vs=False
    mesh=import_asset(root/module/(name+'.fbx'),name,unreal.FbxFactory(),options)
    directions={'XP':(0,0,0),'XN':(0,0,180),'YP':(0,0,90),'YN':(0,0,-90),'ZP':(0,90,0),'ZN':(0,-90,180)}
    for key,pos in data['ports'].items():
        sock=unreal.new_object(unreal.StaticMeshSocket,outer=mesh)
        sock.set_editor_property('socket_name',key)
        sock.set_editor_property('relative_location',unreal.Vector(*pos))
        roll,pitch,yaw=directions[key]
        sock.set_editor_property('relative_rotation',unreal.Rotator(roll=roll,pitch=pitch,yaw=yaw))
        mesh.add_socket(sock)
    dest+='/'+module
    mat=tools.create_asset('M_Surface',dest,unreal.Material,unreal.MaterialFactoryNew())
    for i,(kind,prop) in enumerate([('color',unreal.MaterialProperty.MP_BASE_COLOR),('normal',unreal.MaterialProperty.MP_NORMAL),('metallic',unreal.MaterialProperty.MP_METALLIC),('roughness',unreal.MaterialProperty.MP_ROUGHNESS)]):
        tex=import_asset(root/module/('T_'+kind+'.png'),'T_'+kind,unreal.TextureFactory())
        node=matlib.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-400,i*200)
        if kind!='color':
            tex.set_editor_property('srgb',False)
            tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if kind=='normal' else unreal.TextureCompressionSettings.TC_MASKS)
        node.set_editor_property('texture',tex)
        node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if kind=='color' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        matlib.connect_material_property(node,'RGB' if kind in ('color','normal') else 'R',prop)
        assert library.save_loaded_asset(tex,False)
    matlib.recompile_material(mat)
    mesh.set_material(0,mat)
    assert mesh.find_socket('XN') and mesh.get_num_triangles(0)>100
    assert library.save_loaded_asset(mat,False) and library.save_loaded_asset(mesh,False)
    unreal.log('BASE_IMPORTED '+module+' '+str(mesh.get_bounding_box()))
