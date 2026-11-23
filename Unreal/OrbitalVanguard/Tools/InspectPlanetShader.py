
import unreal
mat = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/M_Planet')
for expr in mat.get_editor_property('expressions'):
    cname = expr.get_class().get_name()
    desc = expr.get_editor_property('desc') if hasattr(expr, 'desc') else ''
    pname = expr.get_editor_property('parameter_name') if hasattr(expr, 'parameter_name') else None
    if 'TextureSample' in cname:
        tex = expr.get_editor_property('texture')
        print('TexSample:', pname, tex.get_name() if tex else None)
    elif pname:
        print('Param:', cname, pname)
