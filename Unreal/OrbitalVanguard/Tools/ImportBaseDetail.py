"""가까운 거리용 원본 삼각형·텍스처와 건설 높이 표현을 기존 기지 자산에 결합한다."""
import json
import os
from pathlib import Path
import unreal as u

root = Path(os.environ['OV_BASE_DETAIL'])
library = u.EditorAssetLibrary
edit = u.MaterialEditingLibrary
meshes = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
tools = u.AssetToolsHelpers.get_asset_tools()
dest = '/Game/Presentation/Base'
report = {}
def build_surface(mat):
    # 완성 메시의 기본값은 전체 표면. 공사 중인 개별 컴포넌트만 높이를 전달한다.
    if 'BuildHeight' in map(str, edit.get_scalar_parameter_names(mat)):
        return
    mat.set_editor_property('blend_mode', u.BlendMode.BLEND_MASKED)
    def node(kind, x, y):
        return edit.create_material_expression(mat, kind, x, y)
    pos = node(u.MaterialExpressionWorldPosition,-800,900)
    def primitive(name, index):
        p = node(u.MaterialExpressionScalarParameter,-800,1100+index*120)
        p.set_editor_property('parameter_name',name)
        p.set_editor_property('use_custom_primitive_data',True)
        p.set_editor_property('primitive_data_index',index)
        return p
    top = primitive('BuildHeight',0)
    active = primitive('UnderConstruction',1)
    def shader(code, prop, color=False):
        n = node(u.MaterialExpressionCustom,-350,1100 if color else 900)
        inputs = []
        for key in ['Pos','Top','Active']:
            item = u.CustomInput()
            item.set_editor_property('input_name',key)
            inputs.append(item)
        n.set_editor_property('inputs', inputs)
        n.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3 if color else u.CustomMaterialOutputType.CMOT_FLOAT1)
        n.set_editor_property('code',code)
        for p,key in [(pos,'Pos'),(top,'Top'),(active,'Active')]:
            assert edit.connect_material_expressions(p,'',n,key)
        assert edit.connect_material_property(n,'',prop)
    shader('return Active < 0.5 || Pos.z <= Top ? 1.0 : 0.0;',u.MaterialProperty.MP_OPACITY_MASK)
    shader('return float3(0.12,1.3,1.8)*Active*saturate(1.0-abs(Pos.z-Top)/25.0);',u.MaterialProperty.MP_EMISSIVE_COLOR,True)
    edit.recompile_material(mat)
    assert library.save_loaded_asset(mat,False)

def import_texture(path, folder, kind):
    name = 'T_'+kind
    existing=library.load_asset(folder+'/'+name)
    if existing and existing.blueprint_get_size_x()>=4096 and str(path) in existing.get_editor_property('asset_import_data').extract_filenames():
        return existing.blueprint_get_size_x()
    t = u.AssetImportTask()
    t.filename = str(path)
    t.destination_path = folder
    t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.factory = u.TextureFactory()
    tools.import_asset_tasks([t])
    tex = library.load_asset(folder+'/'+name)
    assert tex in list(t.get_objects()), path
    tex.set_editor_property('srgb',kind=='color')
    tex.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_DEFAULT if kind=='color' else u.TextureCompressionSettings.TC_NORMALMAP if kind=='normal' else u.TextureCompressionSettings.TC_MASKS)
    assert library.save_loaded_asset(tex,False)
    size=tex.blueprint_get_size_x()
    assert size>=4096,(path,size)
    return size

for stage in (1,2):
    folder = root / ('Stage'+str(stage))
    for name,data in json.loads((folder/'ports.json').read_text()).items():
        filename = 'SM_OV_Base_'+name
        mesh = library.load_asset(dest+'/'+filename)
        old = Path(os.environ.get("OV_BASE_ASSET_ROOT", Path(__file__).resolve().parents[4] / "OrbitalVanguardAssets"))/'Prepared'/('BaseTrial'+str(stage))/name/(filename+'.fbx')
        sockets = list(data['ports'])
        assert all(mesh.find_socket(k) for k in sockets)
        if mesh.get_num_lods()<2:
            assert meshes.import_lod(mesh,1,str(old))==1
        hi = folder/name/(filename+'.fbx')
        if abs(mesh.get_num_triangles(0)-data['triangles'])>200:
            assert meshes.import_lod(mesh,0,str(hi))==0
        assert all(mesh.find_socket(k) for k in sockets)
        assert mesh.get_num_triangles(0)>mesh.get_num_triangles(1)
        assert meshes.set_lod_screen_sizes(mesh,[1.0,0.25])
        assert library.save_loaded_asset(mesh,False)
        matfolder = dest+'/'+name
        src = Path(data['source'])
        sizes = {}
        for kind in ('color','normal','metallic','roughness'):
            path = src.with_name(src.stem+('' if kind=='color' else '_'+kind)+'.png')
            sizes[kind] = import_texture(path,matfolder,kind)
        build_surface(library.load_asset(matfolder+'/M_Surface'))
        report[name] = {'triangles':[mesh.get_num_triangles(i) for i in (0,1)],'maps':sizes}
        (root/'import.json').write_text(json.dumps(report,indent=2))
        u.log('BASE_DETAIL '+name+' '+str(report[name]))
        u.SystemLibrary.collect_garbage()
build_surface(library.load_asset(dest+'/M_PortSteel'))
