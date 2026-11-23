
import unreal
mat = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/M_Cloud_Shadows')
if mat:
    print('M_Cloud_Shadows ShadingModel:', mat.get_editor_property('shading_model'))
    print('M_Cloud_Shadows BlendMode:', mat.get_editor_property('blend_mode'))
