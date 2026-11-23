"""원본 블랙홀 재질의 비굴절 배경과 같은 전방위 은하수 재질을 생성한다."""
import unreal as u

folder = "/Game/Presentation/Environment/SpaceSkybox"
name = "M_MilkyWaySky"
texture = u.load_asset("/Game/BlackHole/Textures/TC_MilkyWay")
source = u.load_asset("/Game/BlackHole/Materials/MI_Blackhole_Physical")
if not texture or not source:
    raise RuntimeError("원본 BlackHole 에셋을 먼저 설치하세요.")
lib = u.MaterialEditingLibrary
mat = u.load_asset(folder+"/"+name) if u.EditorAssetLibrary.does_asset_exist(folder+"/"+name) else u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.Material,u.MaterialFactoryNew())
lib.delete_all_material_expressions(mat)
mat.set_editor_property("shading_model",u.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property("two_sided",True)
mat.set_editor_property("is_sky",True)
def node(cls,x,y):
    return lib.create_material_expression(mat,cls,x,y)
def connect(a,pin,b,slot):
    assert lib.connect_material_expressions(a,pin,b,slot)
eye=node(u.MaterialExpressionCameraVectorWS,-840,0)
direction=node(u.MaterialExpressionMultiply,-630,0)
direction.set_editor_property("const_b",-1.)
connect(eye,"",direction,"A")
sample=node(u.MaterialExpressionTextureSampleParameterCube,-410,0)
sample.set_editor_property("parameter_name","Celestial Disk Texture")
sample.set_editor_property("texture",texture)
sample.set_editor_property("sampler_type",u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if not texture.get_editor_property("srgb") else u.MaterialSamplerType.SAMPLERTYPE_COLOR)
connect(direction,"",sample,"")
power=node(u.MaterialExpressionPower,-160,0)
power.set_editor_property("const_exponent",lib.get_material_instance_scalar_parameter_value(source,"Celestial Disk Exponent"))
connect(sample,"RGB",power,"Base")
gain=node(u.MaterialExpressionMultiply,45,0)
gain.set_editor_property("const_b",lib.get_material_instance_scalar_parameter_value(source,"Celestial Disk Brightness"))
connect(power,"",gain,"A")
assert lib.connect_material_property(gain,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
lib.recompile_material(mat)
assert u.EditorAssetLibrary.save_loaded_asset(mat,False)
