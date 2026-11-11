"""Create missing phase 2A layout assets through UE serialization; preserve existing assets."""
import unreal

properties = ('deck_count', 'small_hull', 'core_slot_id', 'bridge_allowed_slots',
              'slots', 'connections', 'armor_sections', 'weapon_mounts',
              'local_half_extent', 'cannon_slots', 'beam_slots', 'missile_slots',
              'weapon_tier', 'base_radar_range', 'half_height', 'max_hull_hp')
assets = unreal.AssetToolsHelpers.get_asset_tools()
for decks in (1, 2, 3):
    path = '/Game/Layouts/DA_Test%dDeck' % decks
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        unreal.log('OV LAYOUT: preserve existing ' + path)
    else:
        definition = unreal.ShipLayoutDataAsset.make_test_layout(None, decks)
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.ShipLayoutDataAsset)
        asset = assets.create_asset('DA_Test%dDeck' % decks, '/Game/Layouts', unreal.ShipLayoutDataAsset, factory)
        if not asset:
            raise RuntimeError('Could not create ' + path)
        for name in properties:
            asset.set_editor_property(name, definition.get_editor_property(name))
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
    # UE Python consumes bool return as success and returns the out array (None on failure).
    errors = asset.validate_layout()
    if errors is None or len(errors):
        raise RuntimeError(str(errors))
    unreal.log('OV LAYOUT: validated %s armor=%d rooms=%d' %
               (path, len(asset.get_editor_property('armor_sections')), len(asset.get_editor_property('slots'))))
