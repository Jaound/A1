
import unreal
bp = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Blueprints/BP_Planet')
scs = bp.get_editor_property('simple_construction_script')
for node in scs.get_all_nodes():
    comp = node.get_editor_property('component_template')
    print('SCS Node:', node.get_name(), 'Comp:', comp.get_name() if comp else None)
    if isinstance(comp, unreal.StaticMeshComponent):
        print('  Mesh:', comp.get_editor_property('static_mesh'))
        print('  Scale:', comp.get_editor_property('relative_scale3d'))
        for i in range(comp.get_num_materials()):
            print(f'  Mat {i}:', comp.get_material(i))
