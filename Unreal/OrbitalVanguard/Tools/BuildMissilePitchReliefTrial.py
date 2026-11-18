"""미사일 분리 후보 복사본에 임시 회전 공간을 시험한다. 원본을 수정하지 않는다.

이는 임시 Boolean 조형이며 결과의 외관, UV, 연속 회전과 전장 사용을 승인하지 않는다.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

import bmesh
import bpy
from mathutils import Vector

parser = argparse.ArgumentParser()
parser.add_argument('--prefix', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--radius', type=float, required=True)
parser.add_argument('--bearing-radius', type=float, default=0.0)
parser.add_argument('--shape', choices=('sphere', 'cylinder'), default='sphere')
parser.add_argument('--depth', type=float, default=.5)
parser.add_argument('--connector-radius', type=float, default=0.0)
parser.add_argument('--connector-length', type=float, default=.35)
parser.add_argument('--centers-y', type=float, nargs='+', default=[0.0])
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
source = Path(bpy.data.filepath).resolve()
out = Path(args.output).resolve()
if (out.exists() or not source.is_file() or not 0.05 <= args.radius <= 1.8
        or not 0 <= args.bearing_radius < args.radius or not 0.05 <= args.depth <= 1.5
        or not 0 <= args.connector_radius <= .2 or not .05 <= args.connector_length <= 2.5
        or not 1 <= len(args.centers_y) <= 4 or any(abs(y)>1.5 for y in args.centers_y)
        or (args.bearing_radius and len(args.centers_y)!=1)):
    raise ValueError('입력 또는 시험 반지름이 유효하지 않습니다')

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

before = sha(source)
metadata = json.loads((source.parent / 'report.json').read_text(encoding='utf8'))
body = bpy.data.objects.get(args.prefix + '_YawBody')
barrel = bpy.data.objects.get(args.prefix + '_PitchTrial')
if not body or not barrel or barrel.parent != body:
    raise RuntimeError('회전 후보의 계층 구조가 없습니다')
bpy.context.view_layer.update()
pivot = barrel.matrix_world.translation.copy()
def add_relief_shape(radius, depth, center):
    if args.shape == 'sphere':
        bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=24, radius=radius, location=center)
    else:
        bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=radius, depth=depth, location=center,
                                            rotation=(math.pi / 2, 0, 0))

for y in args.centers_y:
    add_relief_shape(args.radius, args.depth, pivot + Vector((0, y, 0)))
    relief = bpy.context.object
    bpy.ops.object.select_all(action='DESELECT')
    body.select_set(True)
    bpy.context.view_layer.objects.active = body
    modifier = body.modifiers.new('ExperimentalPitchRelief', 'BOOLEAN')
    modifier.operation = 'DIFFERENCE'
    modifier.solver = 'EXACT'
    modifier.object = relief
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    bpy.data.objects.remove(relief, do_unlink=True)
if args.bearing_radius:
    add_relief_shape(args.bearing_radius, max(.001, args.depth - .002), pivot + Vector((0, args.centers_y[0], 0)))
    bearing = bpy.context.object
    bearing.name = args.prefix + '_TrialPitchBearing'
    material = bpy.data.materials.new('TrialPitchBearingMaterial')
    material.diffuse_color = (.19, .2, .21, 1)
    bearing.data.materials.append(material)
    for face in bearing.data.polygons:
        face.use_smooth = True
    world = bearing.matrix_world.copy()
    bearing.parent = barrel
    bearing.matrix_world = world
if args.connector_radius:
    # A separate trial neck follows the barrel; it does not replace source artwork.
    material = bpy.data.materials.new('TrialPitchNeckMaterial')
    material.diffuse_color = (.19, .2, .21, 1)
    connectors = []
    for y in args.centers_y:
        center = pivot + Vector((0, y, 0))
        bpy.ops.mesh.primitive_cylinder_add(vertices=48, radius=args.connector_radius,
                                            depth=args.connector_length, location=center,
                                            rotation=(0, math.pi / 2, 0))
        connector = bpy.context.object
        connector.data.materials.append(material)
        connectors.append(connector)
    bpy.ops.object.select_all(action='DESELECT')
    for connector in connectors:
        connector.select_set(True)
    neck = connectors[0]
    bpy.context.view_layer.objects.active = neck
    if len(connectors)>1:
        bpy.ops.object.join()
    neck.name = args.prefix + '_TrialPitchNeck'
    world = neck.matrix_world.copy()
    neck.parent = barrel
    neck.matrix_world = world
bm = bmesh.new()
bm.from_mesh(body.data)
topology = {'faces': len(bm.faces), 'boundary_edges': sum(edge.is_boundary for edge in bm.edges),
            'nonmanifold_edges': sum(not edge.is_manifold for edge in bm.edges)}
bm.free()
out.mkdir(parents=True)
for angle in (0, 15, -15, 30, -30):
    barrel.rotation_euler.x = math.radians(angle)
    bpy.context.view_layer.update()
    bpy.context.scene.render.filepath = str(out / ('pitch_%+03d.png' % angle))
    bpy.ops.render.render(write_still=True)
barrel.rotation_euler.x = 0
bpy.ops.wm.save_as_mainfile(filepath=str(out / (args.prefix + '_MissileReliefTrial.blend')))
after = sha(source)
if before != after:
    raise RuntimeError('입력 파일이 변경되었습니다')
metadata.update({'source': str(source), 'source_before': before, 'source_after': after,
                 'trial_relief_shape': args.shape, 'trial_relief_depth': args.depth,
                 'trial_centers_y': args.centers_y,
                 'trial_relief_radius': args.radius, 'trial_bearing_radius': args.bearing_radius, 'trial_relief_pivot': list(pivot),
                 'trial_connector_radius': args.connector_radius, 'trial_connector_length': args.connector_length,
                 'relief_topology': topology,
                 'status': 'experimental pitch relief; geometry, UV, range and gameplay unapproved'})
(out / 'report.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
print('OV_PITCH_RELIEF_TRIAL', out)
