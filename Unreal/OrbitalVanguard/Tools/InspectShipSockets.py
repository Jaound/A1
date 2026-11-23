import unreal

mesh = unreal.EditorAssetLibrary.load_asset("/Game/Presentation/Ships/DreadnoughtFull/SM_Hull")
if mesh:
    sockets = mesh.get_editor_property('sockets')
    print("Sockets count:", len(sockets))
    for s in sockets:
        name = s.get_editor_property('socket_name')
        loc = s.get_editor_property('relative_location')
        print(f"Socket: {name} at {loc}")

    bounds = mesh.get_bounds()
    print("Bounds origin:", bounds.origin, "box_extent:", bounds.box_extent)
