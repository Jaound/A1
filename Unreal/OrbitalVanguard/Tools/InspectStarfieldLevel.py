import unreal

def inspect_asset():
    # Load level or blueprint
    bp = unreal.EditorAssetLibrary.load_asset("/Game/StarfieldFree/BluePrint/BP_Starfield_Nebula_Free")
    if bp:
        print("BP_Starfield_Nebula_Free loaded:", bp)
    
    # Check meshes
    m1 = unreal.EditorAssetLibrary.load_asset("/Game/StarfieldFree/Mesh/SM_StarSphereFree")
    m2 = unreal.EditorAssetLibrary.load_asset("/Game/StarfieldFree/Mesh/SM_Nebula_Sphere_Free")
    print("Mesh 1 (Stars):", m1)
    print("Mesh 2 (Nebula):", m2)
    
    # Check materials
    mat_stars = unreal.EditorAssetLibrary.load_asset("/Game/StarfieldFree/Materials/Material_Instance/MI_Starfield_Stars_Free")
    mat_nebula = unreal.EditorAssetLibrary.load_asset("/Game/StarfieldFree/Materials/Material_Instance/MI_Starfield_Nebula_Free_04")
    print("Mat Stars:", mat_stars)
    print("Mat Nebula:", mat_nebula)

inspect_asset()
