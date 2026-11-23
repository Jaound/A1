
import unreal
mat = unreal.EditorAssetLibrary.load_asset('/Game/Planet_Project/Assets/Materials/M_Planet')
for expr in mat.get_editor_property('expressions'):
    cname = expr.get_class().get_name()
    pname = expr.get_editor_property('parameter_name') if hasattr(expr, 'parameter_name') else None
    if pname and 'Blend' in str(pname):
        print('Blend param:', cname, pname)
        # Check outputs or inputs
        for prop in dir(expr):
            if 'value' in prop.lower():
                print(' ', prop, getattr(expr, prop, None))
