import json
import os
import unreal

PROJECT_DIR = unreal.Paths.project_dir()
REPORT_PATH = os.path.join(PROJECT_DIR, "Saved", "Validation", "UpgradeProceduralSpaceMaterials.json")


def check(cond, msg, errors):
    if not cond:
        errors.append(msg)
        unreal.log_error("SpaceMatCheck FAILED: " + msg)
    return cond


def build_skybox_hlsl_material(mat_path, errors):
    pkg_path, asset_name = mat_path.rsplit("/", 1)
    if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        mat = unreal.load_asset(mat_path)
    else:
        unreal.EditorAssetLibrary.make_directory(pkg_path)
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = asset_tools.create_asset(asset_name, pkg_path, unreal.Material, unreal.MaterialFactoryNew())

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", True)

    cam_vec = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -600, -100)
    tint_param = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 60)
    tint_param.set_editor_property("parameter_name", "FactionTint")
    tint_param.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    intensity_param = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, 240)
    intensity_param.set_editor_property("parameter_name", "Intensity")
    intensity_param.set_editor_property("default_value", 1.0)

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -240, 0)
    custom.set_editor_property("description", "Procedural3DSpaceSky")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)

    inp_dir = unreal.CustomInput()
    inp_dir.set_editor_property("input_name", "ViewDir")
    inp_tint = unreal.CustomInput()
    inp_tint.set_editor_property("input_name", "Tint")
    inp_int = unreal.CustomInput()
    inp_int.set_editor_property("input_name", "Intensity")
    custom.set_editor_property("inputs", [inp_dir, inp_tint, inp_int])

    hlsl_code = r"""
float3 d = normalize(-ViewDir);

// 3D hash helper (no 2D UV seam, no polar distortion)
float3 p1 = floor(d * 95.0);
float3 f1 = frac(d * 95.0) - 0.5;
float3 h1 = frac(sin(float3(
    dot(p1, float3(127.1, 311.7, 74.7)),
    dot(p1, float3(269.5, 183.3, 246.1)),
    dot(p1, float3(113.5, 271.9, 124.6)))) * 43758.5453);
float3 offset1 = (h1 - 0.5) * 0.68;
float dist1 = length(f1 - offset1);
float star1 = smoothstep(0.095, 0.012, dist1) * step(0.84, h1.x);

float3 p2 = floor(d * 210.0);
float3 f2 = frac(d * 210.0) - 0.5;
float3 h2 = frac(sin(float3(
    dot(p2, float3(127.1, 311.7, 74.7)),
    dot(p2, float3(269.5, 183.3, 246.1)),
    dot(p2, float3(113.5, 271.9, 124.6)))) * 43758.5453);
float3 offset2 = (h2 - 0.5) * 0.72;
float dist2 = length(f2 - offset2);
float star2 = smoothstep(0.08, 0.01, dist2) * step(0.89, h2.y) * 0.65;

// Smooth 3D continuous trigonometric nebula field (zero blockiness)
float n1 = sin(d.x * 3.1 + cos(d.y * 2.7)) * cos(d.z * 2.9 + sin(d.x * 2.3));
float n2 = sin(d.y * 6.3 - cos(d.z * 5.7)) * cos(d.x * 5.9 + sin(d.y * 5.1));
float n3 = sin(d.z * 11.7 + cos(d.x * 10.3)) * cos(d.y * 11.1 - sin(d.z * 9.7));
float neb = saturate(0.5 + 0.28 * n1 + 0.15 * n2 + 0.07 * n3);

// Tilted galactic plane band
float3 galN = normalize(float3(0.28, -0.36, 0.89));
float galPlane = abs(dot(d, galN));
float band = exp(-galPlane * 4.2);
float dustLane = exp(-galPlane * 14.0) * smoothstep(0.42, 0.75, neb);

float3 deepBg = float3(0.0025, 0.0045, 0.010);
float3 nebColA = float3(0.012, 0.032, 0.068) * Tint.rgb;
float3 nebColB = float3(0.038, 0.018, 0.058) * Tint.rgb;
float3 sky = deepBg;
sky += nebColA * (band * smoothstep(0.32, 0.85, neb) * 0.85);
sky += nebColB * (band * smoothstep(0.48, 0.92, 1.0 - neb) * 0.55);
sky *= (1.0 - dustLane * 0.62);

// Distant system sun corona glow
float3 sunDir = normalize(float3(0.74, -0.52, 0.42));
float sunDot = saturate(dot(d, sunDir));
sky += float3(1.0, 0.82, 0.58) * Tint.rgb * (pow(sunDot, 96.0) * 0.45 + pow(sunDot, 12.0) * 0.035);

// Combine pinpoint stars with natural color temperature
float3 starCol1 = lerp(float3(0.72, 0.86, 1.0), float3(1.0, 0.92, 0.78), h1.z);
float3 starCol2 = lerp(float3(0.80, 0.90, 1.0), float3(1.0, 1.0, 1.0), h2.z);
sky += starCol1 * star1 * (1.2 + 0.8 * h1.y) * (0.7 + 0.6 * band);
sky += starCol2 * star2 * (0.8 + 0.5 * band);

return max(sky * max(0.2, Intensity), float3(0.0, 0.0, 0.0));
"""
    custom.set_editor_property("code", hlsl_code)

    check(mel.connect_material_expressions(cam_vec, "", custom, "ViewDir"), f"{mat_path}: ViewDir", errors)
    check(mel.connect_material_expressions(tint_param, "", custom, "Tint"), f"{mat_path}: Tint", errors)
    check(mel.connect_material_expressions(intensity_param, "", custom, "Intensity"), f"{mat_path}: Intensity", errors)
    check(mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), f"{mat_path}: MP_EMISSIVE_COLOR", errors)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)


def build_planet_hlsl_material(mat_path, errors):
    pkg_path, asset_name = mat_path.rsplit("/", 1)
    if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        mat = unreal.load_asset(mat_path)
    else:
        unreal.EditorAssetLibrary.make_directory(pkg_path)
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = asset_tools.create_asset(asset_name, pkg_path, unreal.Material, unreal.MaterialFactoryNew())

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", False)

    wnorm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -600, -140)
    cam_vec = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -600, 0)
    tint_param = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -600, 140)
    tint_param.set_editor_property("parameter_name", "FactionTint")
    tint_param.set_editor_property("default_value", unreal.LinearColor(0.55, 0.80, 1.0, 1.0))

    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -220, 0)
    custom.set_editor_property("description", "Procedural3DPlanet")
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)

    inp_n = unreal.CustomInput()
    inp_n.set_editor_property("input_name", "WNormal")
    inp_v = unreal.CustomInput()
    inp_v.set_editor_property("input_name", "ViewDir")
    inp_t = unreal.CustomInput()
    inp_t.set_editor_property("input_name", "Tint")
    custom.set_editor_property("inputs", [inp_n, inp_v, inp_t])

    hlsl_code = r"""
float3 n = normalize(WNormal);
float3 v = normalize(ViewDir);
float3 sunDir = normalize(float3(-0.65, 0.55, 0.52));

// Procedural planetary atmospheric bands in 3D normal space
float lat = n.z * 6.5 + 0.35 * sin(n.x * 5.0 + n.y * 4.0);
float band1 = 0.5 + 0.5 * sin(lat * 3.2);
float band2 = 0.5 + 0.5 * cos(lat * 7.5 + sin(n.y * 8.0));
float3 surfA = float3(0.05, 0.09, 0.16) * Tint.rgb;
float3 surfB = float3(0.14, 0.22, 0.32) * Tint.rgb;
float3 surface = lerp(surfA, surfB, 0.6 * band1 + 0.4 * band2);

// Day/night crescent terminator
float ndotl = dot(n, sunDir);
float lit = smoothstep(-0.12, 0.45, ndotl);
float3 dayCol = surface * (0.12 + 1.35 * lit);

// Fresnel atmospheric rim light
float ndotv = saturate(dot(n, v));
float rim = pow(1.0 - ndotv, 3.6);
float3 atmoCol = float3(0.28, 0.62, 1.0) * Tint.rgb * rim * (0.25 + 1.25 * smoothstep(-0.35, 0.5, ndotl));

return dayCol + atmoCol;
"""
    custom.set_editor_property("code", hlsl_code)

    check(mel.connect_material_expressions(wnorm, "", custom, "WNormal"), f"{mat_path}: WNormal", errors)
    check(mel.connect_material_expressions(cam_vec, "", custom, "ViewDir"), f"{mat_path}: ViewDir", errors)
    check(mel.connect_material_expressions(tint_param, "", custom, "Tint"), f"{mat_path}: Tint", errors)
    check(mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), f"{mat_path}: MP_EMISSIVE_COLOR", errors)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)


def build_starpoint_material(mat_path, errors):
    pkg_path, asset_name = mat_path.rsplit("/", 1)
    if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        mat = unreal.load_asset(mat_path)
    else:
        unreal.EditorAssetLibrary.make_directory(pkg_path)
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = asset_tools.create_asset(asset_name, pkg_path, unreal.Material, unreal.MaterialFactoryNew())

    mel = unreal.MaterialEditingLibrary
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("used_with_instanced_static_meshes", True)

    tint_param = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -420, -60)
    tint_param.set_editor_property("parameter_name", "FactionTint")
    tint_param.set_editor_property("default_value", unreal.LinearColor(0.90, 0.96, 1.0, 1.0))

    intensity_param = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -420, 120)
    intensity_param.set_editor_property("parameter_name", "Intensity")
    intensity_param.set_editor_property("default_value", 3.5)

    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -160, 0)
    check(mel.connect_material_expressions(tint_param, "RGB", mul, "A"), f"{mat_path}: Tint->Mul.A", errors)
    check(mel.connect_material_expressions(intensity_param, "", mul, "B"), f"{mat_path}: Int->Mul.B", errors)
    check(mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR), f"{mat_path}: MP_EMISSIVE_COLOR", errors)

    mel.layout_material_expressions(mat)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)


def main():
    errors = []
    build_skybox_hlsl_material("/Game/Presentation/Environment/SpaceSkybox/M_Source", errors)
    build_planet_hlsl_material("/Game/Presentation/Environment/SpaceSkybox/M_Planet", errors)
    build_starpoint_material("/Game/Presentation/Environment/SpaceSkybox/M_StarPoint", errors)

    os.makedirs(os.path.dirname(REPORT_PATH), exist_ok=True)
    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        json.dump({"passed": len(errors) == 0, "errors": errors}, f, indent=2)
    if errors:
        raise RuntimeError("UpgradeProceduralSpaceMaterials failed: " + "; ".join(errors))
    unreal.log("UpgradeProceduralSpaceMaterials succeeded!")


if __name__ == "__main__":
    main()
