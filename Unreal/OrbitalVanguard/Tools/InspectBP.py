import unreal

bp = unreal.EditorAssetLibrary.load_asset("/Game/StarfieldFree/BluePrint/BP_Starfield_Nebula_Free")
if bp:
    print("BP_LOADED:", bp.get_name())
    cdo = unreal.get_default_object(bp.generated_class())
    for comp in cdo.get_editor_property("root_component").get_children_components(True) if hasattr(cdo, "root_component") else []:
        print("COMP:", comp)

unreal.SystemLibrary.execute_console_command(None, "Quit")
