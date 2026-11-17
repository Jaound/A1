"""로컬 원본을 변경하지 않고 모델/프로토타입 출처를 조사한다."""
from pathlib import Path
import hashlib
import json
import zipfile

root = Path.home() / 'Documents/OrbitalVanguardAssets'
reports = root / 'Reports'
reports.mkdir(parents=True, exist_ok=True)

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

items = []
for path in sorted((root / 'Incoming').glob('*.zip')):
    with zipfile.ZipFile(path) as archive:
        entries = [{'path': i.filename, 'bytes': i.file_size} for i in archive.infolist() if not i.is_dir()]
    items.append({'path': str(path), 'bytes': path.stat().st_size, 'sha256': sha(path),
                  'lfs_pointer': False, 'entries': entries})
sources = [{'path': str(p), 'bytes': p.stat().st_size, 'sha256': sha(p)}
           for p in (root / 'Source').rglob('*.fbx')]
data = {'description': '実バイト確認。原本非変更。FBX/テクスチャ接続の視覚検証とは別。', 'archives': items, 'fbx': sources}
data['description'] = '실제 원본 바이트 확인·원본 비변경. FBX/텍스처 연결의 시각 검증과는 별도.'
(reports / 'PlayUI_SourceInventory.json').write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding='utf-8')
prototype = Path.home() / 'Downloads/Play_Game_3.zip'
if prototype.exists():
    target = reports / 'Prototype_ReadOnly'
    target.mkdir(exist_ok=True)
    entries = []
    with zipfile.ZipFile(prototype) as archive:
        for name in archive.namelist():
            if Path(name).name in ('MainGame.gd', 'BoardingBlueprintPanel.gd', 'ShipMeshFactory.gd'):
                content = archive.read(name)
                digest = hashlib.sha256(content).hexdigest()
                destination = target / (Path(name).stem + '_' + digest[:10] + '.gd')
                if not destination.exists():
                    destination.write_bytes(content)
                entries.append({'archive_path': name, 'path': str(destination), 'sha256': digest})
    result = {'archive': str(prototype), 'sha256': sha(prototype), 'sources': entries}
    (reports / 'PlayUI_Prototype.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(result, ensure_ascii=False))
print(json.dumps({'archives': len(items), 'fbx': len(sources), 'report': str(reports / 'PlayUI_SourceInventory.json')}, ensure_ascii=False))
