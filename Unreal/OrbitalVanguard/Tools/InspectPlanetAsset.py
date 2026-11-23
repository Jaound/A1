import unreal

out_path = r"C:\Users\user\.gemini\antigravity\brain\0da5275e-6784-4e30-ab80-f739659cc0ff\m_planet_params.txt"
with open(out_path, "w", encoding="utf-8") as f:
    mat = unreal.EditorAssetLibrary.load_asset("/Game/Planet_Project/Assets/Materials/M_Planet")
    if mat:
        for exp in mat.get_editor_property("expressions"):
            name = exp.get_class().get_name()
            if "ScalarParameter" in name:
                param_name = exp.get_editor_property("parameter_name")
                default_val = exp.get_editor_property("default_value")
                f.write(f"ScalarParam: {param_name} = {default_val}\n")
            elif "VectorParameter" in name:
                param_name = exp.get_editor_property("parameter_name")
                default_val = exp.get_editor_property("default_value")
                f.write(f"VectorParam: {param_name} = {default_val}\n")
