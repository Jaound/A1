"""Create a second, source-map trial material with a modest visibility fill.

The first material and original maps remain untouched. This is a preview policy,
not final art or a substitute for evaluating battlefield lighting.
"""
import json
import os
from pathlib import Path
import traceback
import unreal

full_maps=os.environ.get('OV_PC14_FULL_MAPS')=='1'
destination='/Game/Presentation/PC14Full' if full_maps else '/Game/Presentation/PC14Trial'
name=os.environ.get('OV_PC14_TRIAL_MATERIAL_NAME','M_PC14_TrialAmbient')
fill_value=float(os.environ.get('OV_PC14_TRIAL_FILL','.35'))
out=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()))/('Validation/'+name+'.json')
report={'passed':False,'checks':[],'errors':[],'ambient_fill':fill_value,'scope':('source-resolution' if full_maps else 'reduced')+' source maps and provisional emissive fill'}
def check(key, value):
    report['checks'].append({'id':key,'passed':bool(value)})
    if not value: raise AssertionError(key)
try:
    check('NO-OVERWRITE',not unreal.EditorAssetLibrary.does_asset_exist(destination+'/'+name))
    textures={}
    suffix='Full' if full_maps else 'Trial'
    for kind,asset in [('base','T_PC14_Base_'+suffix),('normal','T_PC14_Normal_'+suffix),('roughness','T_PC14_Roughness_'+suffix),('metallic','T_PC14_Metallic_'+suffix)]:
        textures[kind]=unreal.EditorAssetLibrary.load_asset(destination+'/'+asset)
        check('TEXTURE-'+kind,isinstance(textures[kind],unreal.Texture2D))
    tools=unreal.AssetToolsHelpers.get_asset_tools()
    mat=tools.create_asset(name,destination,unreal.Material,unreal.MaterialFactoryNew())
    check('MATERIAL',isinstance(mat,unreal.Material))
    lib=unreal.MaterialEditingLibrary;nodes={}
    for i,kind in enumerate(('base','normal','roughness','metallic')):
        node=lib.create_material_expression(mat,unreal.MaterialExpressionTextureSample,-700,i*220)
        node.set_editor_property('texture',textures[kind])
        node.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='normal' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS if kind!='base' else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        nodes[kind]=node
    tint=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-700,-220)
    tint.set_editor_property('parameter_name','FactionTint');tint.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
    tinted=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply,-360,-80)
    albedo=nodes['base']; channel='RGB'
    if name=='M_PC14_FieldReadable':
        # 선체 도장의 어두운 중간톤만 올리고, 노멀·거칠기·금속도는 원본을 쓴다.
        lifted=lib.create_material_expression(mat,unreal.MaterialExpressionPower,-510,-110)
        lifted.set_editor_property('const_exponent',.58)
        check('SHADOW-LIFT',lib.connect_material_expressions(albedo,'RGB',lifted,'Base'))
        albedo=lifted;channel=''
    check('TINT',lib.connect_material_expressions(albedo,channel,tinted,'A') and lib.connect_material_expressions(tint,'RGB',tinted,'B'))
    check('BASE',lib.connect_material_property(tinted,'',unreal.MaterialProperty.MP_BASE_COLOR))
    for kind,prop in [('normal',unreal.MaterialProperty.MP_NORMAL),('roughness',unreal.MaterialProperty.MP_ROUGHNESS),('metallic',unreal.MaterialProperty.MP_METALLIC)]:
        check('CONNECT-'+kind,lib.connect_material_property(nodes[kind],'RGB' if kind=='normal' else 'R',prop))
    fill=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-350,-300)
    fill.set_editor_property('r',fill_value)
    emission=lib.create_material_expression(mat,unreal.MaterialExpressionMultiply,-100,-180)
    check('FILL',lib.connect_material_expressions(tinted,'',emission,'A') and lib.connect_material_expressions(fill,'',emission,'B') and lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR))
    lib.recompile_material(mat)
    check('SAVE',unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False))
    report['passed']=True
except Exception:
    report['errors'].append(traceback.format_exc());unreal.log_error(report['errors'][-1])
finally:
    out.parent.mkdir(parents=True,exist_ok=True);out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.SystemLibrary.quit_editor()
