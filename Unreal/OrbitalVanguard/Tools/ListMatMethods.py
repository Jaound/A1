import unreal

methods = [m for m in dir(unreal.MaterialEditingLibrary) if not m.startswith("_")]
with open("C:/Users/user/source/repos/A1/Unreal/OrbitalVanguard/Tools/mat_lib_methods.txt", "w") as f:
    f.write("\n".join(methods))
