
import unreal
mat = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/M_Clouds')
for expr in mat.get_editor_property('expressions'):
    cname = expr.get_class().get_name()
    pname = expr.get_editor_property('parameter_name') if hasattr(expr, 'parameter_name') else None
    if pname and 'Opacity' in str(pname):
        print('Opacity param expression:', cname, pname)
