import unreal

lines = []
for lvl in ["L_Milkyway", "L_Starfield_Free_01", "L_Starfield_Free_02", "L_Starfield_Free_03", "L_Starfield_Free_04", "L_Starfield_Free_05"]:
    path = f"/Game/StarfieldFree/Levels/{lvl}.{lvl}:PersistentLevel"
    world = unreal.EditorAssetLibrary.load_asset(f"/Game/StarfieldFree/Levels/{lvl}")
    lines.append(f"Level: {lvl} -> {world}")

with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/level_info.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
