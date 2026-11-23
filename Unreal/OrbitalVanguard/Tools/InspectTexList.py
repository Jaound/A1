import unreal

for tex_name in ["T_Nebula_Free_01_8k", "T_Nebula_Free_02_8k", "T_Nebula_Free_03_8k", "T_Nebula_Free_04_8k", "T_Nebula_Free_05_8k", "T_Milky_Way_8k", "T_Visible_Stars_8k"]:
    tex = unreal.EditorAssetLibrary.load_asset(f"/Game/StarfieldFree/Textures/Nebula/{tex_name}")
    if not tex:
        tex = unreal.EditorAssetLibrary.load_asset(f"/Game/StarfieldFree/Textures/Real_Stars/{tex_name}")
    print(f"{tex_name}: {tex}")
