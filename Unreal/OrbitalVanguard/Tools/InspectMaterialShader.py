
import unreal

for mat_path in [
    '/Game/Planet_Project/Assets/Materials/MI_Clouds',
    '/Game/Planet_Project/Assets/Materials/MI_Planet',
    '/Game/Planet_Project/Assets/Materials/MI_Atmosphere',
    '/Game/Planet_Project/Assets/Materials/M_Clouds',
    '/Game/Planet_Project/Assets/Materials/M_Planet'
]:
    mat = unreal.EditorAssetLibrary.load_asset(mat_path)
    if mat:
        print('=== ' + mat_path + ' ===')
        print('Class:', mat.get_class().get_name())
        if isinstance(mat, unreal.Material):
            print('ShadingModel:', mat.get_editor_property('shading_model'))
            print('BlendMode:', mat.get_editor_property('blend_mode'))
        elif isinstance(mat, unreal.MaterialInstance):
            print('Parent:', mat.get_editor_property('parent'))
