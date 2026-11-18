"""별도 준비한 9종 FBX를 재반입해 원본/텍스처 해시와 메시 수를 검증한다."""
import hashlib
import json
from pathlib import Path
import bpy

root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/PlayUI_R2_WeaponFullSources_v1'
project = Path(__file__).resolve().parents[1]
out = project / 'Saved/Validation/WeaponFullSourcesValidation.json'
if out.exists():
    raise FileExistsError(out)
data = json.loads((root/'report.json').read_text(encoding='utf8'))
def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()
rows=[]
for entry in data['models']:
    original=Path(entry['source_fbx']);prepared=Path(entry['prepared_fbx'])
    source_ok=sha(original)==entry['source_sha256'];prepared_ok=sha(prepared)==entry['prepared_sha256']
    texture_ok=all(sha(prepared.parent/t['file'])==t['sha256'] for t in entry['textures'])
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(prepared),use_image_search=False)
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    vertices=sum(len(o.data.vertices) for o in meshes);polygons=sum(len(o.data.polygons) for o in meshes)
    source_vertices=sum(m['vertices'] for m in entry['meshes']);source_polygons=sum(m['polygons'] for m in entry['meshes'])
    row={'id':entry['id'],'source_unchanged':source_ok,'prepared_hash_ok':prepared_ok,'textures_hash_ok':texture_ok,
         'source_vertices':source_vertices,'prepared_vertices':vertices,'source_polygons':source_polygons,
         'prepared_polygons':polygons,'uv_channels':sum(len(o.data.uv_layers) for o in meshes)}
    row['passed']=source_ok and prepared_ok and texture_ok and vertices==source_vertices and polygons==source_polygons and row['uv_channels']>0
    rows.append(row)
    print('OV_WEAPON_VERIFY',entry['id'],row['passed'],vertices,polygons,flush=True)
out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'scope':'원본 해시와 별도 FBX 재반입 구조. UE 적용·가동·재질 품질 검증 아님.',
                           'passed':len(rows)==9 and all(x['passed'] for x in rows),'models':rows},ensure_ascii=False,indent=2)+'\n',encoding='utf8')
