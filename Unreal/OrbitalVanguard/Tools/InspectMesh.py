import unreal
mat = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/M_Planet')
if mat:
    unreal.log_warning('M_PLANET LOADED')
    for p in mat.get_editor_property('scalar_parameter_values') if hasattr(mat, 'get_editor_property') else []:
        unreal.log_warning('M_PLANET SCALAR: ' + str(p.parameter_info.name) + ' = ' + str(p.parameter_value))

inst = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/MI_Planet')
if inst:
    unreal.log_warning('MI_PLANET DEFAULT PARAMS:')
    for p in inst.scalar_parameter_values:
        unreal.log_warning('  SCALAR: ' + str(p.parameter_info.name) + ' = ' + str(p.parameter_value))
    for p in inst.vector_parameter_values:
        unreal.log_warning('  VECTOR: ' + str(p.parameter_info.name) + ' = ' + str(p.parameter_value.r) + ', ' + str(p.parameter_value.g) + ', ' + str(p.parameter_value.b))

unreal.SystemLibrary.execute_console_command(None, 'Quit')
