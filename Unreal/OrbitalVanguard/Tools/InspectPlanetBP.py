
import unreal
planet_mat = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/M_Planet')
cloud_mat = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/M_Clouds')
print('M_Planet ShadingModel:', planet_mat.get_editor_property('shading_model'))
print('M_Planet BlendMode:', planet_mat.get_editor_property('blend_mode'))
print('M_Clouds ShadingModel:', cloud_mat.get_editor_property('shading_model'))
print('M_Clouds BlendMode:', cloud_mat.get_editor_property('blend_mode'))
