"""Blender background inspection of the prepared PC14; does not modify the blend.

The result uses the current presentation fitting formula and candidate corvette
local mount coordinates. FBX-to-UE axis conversion still requires UE verification.
"""
import json
import os
import sys

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

args = sys.argv[sys.argv.index('--') + 1:]
if len(args) != 1:
    raise ValueError('Pass a new JSON report path after --')
output = os.path.abspath(args[0])
if os.path.exists(output):
    raise FileExistsError(output)
mesh = next(obj for obj in bpy.context.scene.objects if obj.name == 'OV_PC14_Preview' and obj.type == 'MESH')
corners = [mesh.matrix_world @ Vector(corner) for corner in mesh.bound_box]
lower = Vector(tuple(min(p[i] for p in corners) for i in range(3)))
upper = Vector(tuple(max(p[i] for p in corners) for i in range(3)))
center = (lower + upper) / 2
mesh_half_cm = (upper - lower) * 50
obb_half_cm = Vector((175, 75, 30))
scale = min(obb_half_cm[i] / mesh_half_cm[i] for i in range(3))
tree = BVHTree.FromObject(mesh, bpy.context.evaluated_depsgraph_get())
mounts = []
for mount_id, x, side in [('Weapon_0', -46.6666667, 1), ('Weapon_1', 46.6666667, -1)]:
    origin = Vector((center.x + x / (100 * scale), center.y, upper.z + 1 if side > 0 else lower.z - 1))
    hit, _, _, _ = tree.ray_cast(origin, Vector((0, 0, -side)), 20)
    surface_z = None if hit is None else (hit.z - center.z) * 100 * scale
    mounts.append({'id': mount_id, 'local_x_cm': x, 'side': 'top' if side > 0 else 'bottom',
                   'game_mount_z_cm': side * 42, 'surface_z_cm': surface_z,
                   'origin_to_surface_cm': None if surface_z is None else abs(side * 42 - surface_z)})
result = {'scope': 'Blender prepared mesh surface rays, before FBX-to-UE conversion',
          'source_modified': False, 'obb_half_extent_cm': list(obb_half_cm),
          'prepared_mesh_half_extent_cm': list(mesh_half_cm), 'presentation_uniform_scale': scale,
          'mounts': mounts, 'limitations': ['No collision, pivot, muzzle or axis validation in Unreal',
                                           'Simplified mesh is not the final source-quality approval']}
os.makedirs(os.path.dirname(output), exist_ok=True)
with open(output, 'x', encoding='utf-8') as stream:
    json.dump(result, stream, ensure_ascii=False, indent=2)
print('OV_PC14_SURFACE_REPORT', output)
