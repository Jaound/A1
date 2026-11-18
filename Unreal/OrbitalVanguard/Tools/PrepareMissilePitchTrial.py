"""미사일 발사기 회전 후보. 원본은 보존하고 절단 결과는 별도 검사한다."""
import argparse, hashlib, json, math, sys
from pathlib import Path
import bmesh, bpy
from mathutils import Matrix, Vector

p = argparse.ArgumentParser()
for key in ('prefix', 'evidence', 'output'):
    p.add_argument('--' + key, required=True)
for key in ('cut-z', 'pivot-y', 'pivot-z'):
    p.add_argument('--' + key, required=True, type=float)
a = p.parse_args(sys.argv[sys.argv.index('--') + 1:])
source, evidence, out = Path(bpy.data.filepath).resolve(), Path(a.evidence).resolve(), Path(a.output).resolve()
if not source.is_file() or not evidence.is_file() or out.exists():
    raise RuntimeError('입력 또는 새 출력 폴더 확인 필요')
def sha(path):
    with path.open('rb') as f: return hashlib.file_digest(f, 'sha256').hexdigest()
before = sha(source)
scan = json.loads(evidence.read_text(encoding='utf8'))
if scan.get('source_before') != before or scan.get('source_after') != before or scan.get('axis') != 'z' or scan.get('mesh') != a.prefix + '_YawTrial':
    raise RuntimeError('입력 메시와 단면 근거가 일치하지 않습니다')
original = bpy.data.objects.get(a.prefix + '_YawTrial')
base = bpy.data.objects.get(a.prefix + '_BaseTrial')
if not original or not base: raise RuntimeError('회전 후보가 없습니다')
out.mkdir(parents=True)
yaw_origin = original.matrix_world.translation.copy()
pivot = Vector((0, a.pivot_y, a.pivot_z))
cap = bpy.data.materials.new('MissileTrialCutCap')
cap.diffuse_color = (.22, .23, .25, 1)
parts, results = {}, {}
for suffix, keep_top in [('YawBody', False), ('PitchTrial', True)]:
    mesh = original.data.copy()
    obj = bpy.data.objects.new(a.prefix + '_' + suffix, mesh)
    bpy.context.scene.collection.objects.link(obj)
    mesh.materials.append(cap)
    bm = bmesh.new(); bm.from_mesh(mesh); bm.transform(original.matrix_world)
    cut = bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-5,
                                 plane_co=Vector((0, 0, a.cut_z)), plane_no=Vector((0, 0, 1)),
                                 clear_inner=keep_top, clear_outer=not keep_top)
    edges = [e for e in cut['geom_cut'] if isinstance(e, bmesh.types.BMEdge) and e.is_valid and e.is_boundary]
    filled = bmesh.ops.holes_fill(bm, edges=edges, sides=0) if edges else {'faces': []}
    for face in filled['faces']: face.material_index = len(mesh.materials) - 1
    results[suffix] = {'faces': len(bm.faces), 'cut_edges': len(edges), 'cap_faces': len(filled['faces']),
                       'boundary_edges': sum(e.is_boundary for e in bm.edges),
                       'nonmanifold_edges': sum(not e.is_manifold for e in bm.edges)}
    bm.to_mesh(mesh); bm.free()
    origin = pivot if keep_top else yaw_origin
    mesh.transform(Matrix.Translation(-origin)); obj.location = origin; parts[suffix] = obj
bpy.data.objects.remove(original, do_unlink=True)
body, launcher = parts['YawBody'], parts['PitchTrial']
bpy.context.view_layer.update(); world = launcher.matrix_world.copy(); launcher.parent = body; launcher.matrix_world = world
scene = bpy.context.scene
cam = bpy.data.cameras.new('MissileTrialCamera'); camera = bpy.data.objects.new('MissileTrialCamera', cam)
scene.collection.objects.link(camera); scene.camera = camera; cam.type = 'ORTHO'; cam.ortho_scale = 5
camera.location = Vector((5, -7, 3)); camera.rotation_euler = (Vector((0, 0, .2)) - camera.location).to_track_quat('-Z', 'Y').to_euler()
scene.render.engine = 'BLENDER_WORKBENCH'; scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'; scene.display.shading.show_shadows = True
scene.display.shading.show_cavity = True; scene.display.shading.background_type = 'WORLD'
if not scene.world: scene.world = bpy.data.worlds.new('MissileTrialWorld')
scene.world.color = (.025, .035, .05); scene.render.resolution_x = scene.render.resolution_y = 800
for angle in (0, 15, -15):
    launcher.rotation_euler.x = math.radians(angle); bpy.context.view_layer.update()
    scene.render.filepath = str(out / ('pitch_%+03d.png' % angle)); bpy.ops.render.render(write_still=True)
launcher.rotation_euler.x = 0
bpy.ops.wm.save_as_mainfile(filepath=str(out / (a.prefix + '_MissilePitchTrial.blend')))
after = sha(source)
(out / 'report.json').write_text(json.dumps({'source': str(source), 'source_before': before, 'source_after': after,
    'evidence': str(evidence), 'evidence_sha256': sha(evidence), 'cut_z': a.cut_z, 'trial_pivot': list(pivot),
    'trial_yaw_pivot': list(yaw_origin), 'parts': results, 'status': '실험용 절단. 교차·발사구·게임 미검증'},
    ensure_ascii=False, indent=2) + '\n', encoding='utf8')
if before != after: raise RuntimeError('입력이 변경되었습니다')
print('OV_MISSILE_PITCH_TRIAL', out)
