
import unreal

bp = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Blueprints/BP_Planet')
gen_class = bp.generated_class()
cdo = unreal.get_default_object(gen_class)
print('BP_Planet CDO:', cdo)

# List all component templates in the Blueprint
for comp in bp.get_editor_property('component_templates'):
    print('Component Template:', comp.get_name(), comp.get_class().get_name())
    if isinstance(comp, unreal.StaticMeshComponent):
        print('  Mesh:', comp.get_editor_property('static_mesh'))
        print('  Scale:', comp.get_editor_property('relative_scale3d'))
        for i in range(comp.get_num_materials()):
            print(f'  Mat {i}:', comp.get_material(i))
