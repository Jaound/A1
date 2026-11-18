"""미사일 절단의 시험용 덮개만 제거한 복사본을 시각 비교한다.

개방 경계를 만드는 실험이며 게임용 자산이 아니다. 입력 메시를 수정하지 않는다.
"""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bmesh, bpy

p = argparse.ArgumentParser()
p.add_argument('--prefix', required=True); p.add_argument('--output', required=True)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
source, out = Path(bpy.data.filepath).resolve(), Path(a.output).resolve()
if not source.is_file() or out.exists(): raise RuntimeError('입력 또는 새 출력 경로 확인 필요')
def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()
before = sha(source)
body = bpy.data.objects.get(a.prefix + '_YawBody'); launcher = bpy.data.objects.get(a.prefix + '_PitchTrial')
if not body or not launcher or launcher.parent != body: raise RuntimeError('후보 계층 누락')
results = {}
for obj in (body, launcher):
    indices = [i for i, mat in enumerate(obj.data.materials) if mat and mat.name.startswith('MissileTrialCutCap')]
    if len(indices) != 1: raise RuntimeError('시험용 절단 재질 누락')
    bm = bmesh.new(); bm.from_mesh(obj.data)
    faces = [f for f in bm.faces if f.material_index in indices]
    count = len(faces)
    if not count: raise RuntimeError('시험용 덮개 없음')
    bmesh.ops.delete(bm, geom=faces, context='FACES_ONLY')
    results[obj.name] = {'removed_trial_cap_faces': count, 'boundary_edges': sum(e.is_boundary for e in bm.edges),
                         'nonmanifold_edges': sum(not e.is_manifold for e in bm.edges)}
    bm.to_mesh(obj.data); bm.free()
out.mkdir(parents=True)
for angle in (0, 15, -15, 30, -30):
    launcher.rotation_euler.x = math.radians(angle); bpy.context.view_layer.update()
    bpy.context.scene.render.filepath = str(out / ('pitch_%+03d.png' % angle)); bpy.ops.render.render(write_still=True)
launcher.rotation_euler.x = 0
bpy.ops.wm.save_as_mainfile(filepath=str(out / (a.prefix + '_OpenSeamTrial.blend')))
after = sha(source)
(out / 'report.json').write_text(json.dumps({'source': str(source), 'source_before': before, 'source_after': after,
    'parts': results, 'status': '절단 덮개 제거 비교. 개방 경계와 접합 미검증, 게임 미적용'}, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
if before != after: raise RuntimeError('입력이 변경되었습니다')
print('OV_MISSILE_OPEN_SEAM_TRIAL', out)
