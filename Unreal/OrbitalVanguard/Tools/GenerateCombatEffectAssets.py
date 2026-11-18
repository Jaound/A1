"""Blender 5.2 script to generate 3D meshes (.fbx) and PBR/Emissive textures (.png) for Cannon Shells, Laser Beams, Impact Bursts, and Shield Grid."""
import math
from pathlib import Path
import struct
import zlib
import bpy
import bmesh

out_root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/CombatEffects_v1'
out_root.mkdir(parents=True, exist_ok=True)


def write_png(path: Path, width: int, height: int, rgb_bytes: bytes):
    """Write raw RGB8 bytes to a valid PNG file without external dependencies."""
    raw_rows = bytearray()
    stride = width * 3
    for y in range(height):
        raw_rows.append(0)
        raw_rows.extend(rgb_bytes[y * stride:(y + 1) * stride])

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    compressed = zlib.compress(bytes(raw_rows), 6)
    png_data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) + chunk(b'IDAT', compressed) + chunk(b'IEND', b'')
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png_data)


def clamp01(v: float) -> float:
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)


def clear_scene():
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    for m in list(bpy.data.meshes):
        bpy.data.meshes.remove(m)


def add_cylindrical_uv(mesh):
    uv_layer = mesh.uv_layers.new(name='UVMap')
    mesh.uv_layers.active = uv_layer
    for poly in mesh.polygons:
        for loop_idx in poly.loop_indices:
            v = mesh.vertices[mesh.loops[loop_idx].vertex_index].co
            angle = math.atan2(v.y, v.x)
            u = (angle + math.pi) / (2.0 * math.pi)
            v_coord = clamp01((v.z + 50.0) / 100.0)
            uv_layer.data[loop_idx].uv = (u, v_coord)
    for vert in mesh.vertices:
        vert.co *= 0.01


def add_spherical_uv(mesh):
    uv_layer = mesh.uv_layers.new(name='UVMap')
    mesh.uv_layers.active = uv_layer
    for poly in mesh.polygons:
        for loop_idx in poly.loop_indices:
            v = mesh.vertices[mesh.loops[loop_idx].vertex_index].co
            r = max(1e-4, v.length)
            u = (math.atan2(v.y, v.x) + math.pi) / (2.0 * math.pi)
            v_coord = clamp01(math.acos(max(-1.0, min(1.0, v.z / r))) / math.pi)
            uv_layer.data[loop_idx].uv = (u, v_coord)
    for vert in mesh.vertices:
        vert.co *= 0.01


def build_cannon_shell(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    clear_scene()
    mesh = bpy.data.meshes.new('CannonShellMesh')
    bm = bmesh.new()

    # Profile along Z from -50 (tail) to +50 (warhead tip), max radius ~34
    profile = [
        (-50.0, 6.0),
        (-44.0, 22.0),
        (-34.0, 16.0),
        (-26.0, 28.0),
        (-14.0, 29.0),
        (-8.0, 24.0),   # plasma groove 1
        (-2.0, 30.0),
        (8.0, 30.0),
        (13.0, 24.5),   # plasma groove 2
        (19.0, 29.0),
        (30.0, 23.0),
        (40.0, 13.0),
        (47.0, 5.0),
        (50.0, 0.5),
    ]
    segs = 20
    rings = []
    for z, rad in profile:
        ring = []
        for i in range(segs):
            ang = 2.0 * math.pi * i / segs
            ring.append(bm.verts.new((rad * math.cos(ang), rad * math.sin(ang), z)))
        rings.append(ring)

    for r_idx in range(len(rings) - 1):
        r0, r1 = rings[r_idx], rings[r_idx + 1]
        for i in range(segs):
            ni = (i + 1) % segs
            bm.faces.new((r0[i], r0[ni], r1[ni], r1[i]))

    # Cap tail and nose
    tail_center = bm.verts.new((0.0, 0.0, -50.0))
    nose_center = bm.verts.new((0.0, 0.0, 50.0))
    for i in range(segs):
        ni = (i + 1) % segs
        bm.faces.new((rings[0][ni], rings[0][i], tail_center))
        bm.faces.new((rings[-1][i], rings[-1][ni], nose_center))

    # Add 4 stabilizer fins (-46 to -16)
    for f_idx in range(4):
        ang = f_idx * (math.pi * 0.5)
        ca, sa = math.cos(ang), math.sin(ang)
        px, py = -sa * 3.2, ca * 3.2
        v0 = bm.verts.new((16.0 * ca + px, 16.0 * sa + py, -46.0))
        v1 = bm.verts.new((40.0 * ca + px * 0.4, 40.0 * sa + py * 0.4, -42.0))
        v2 = bm.verts.new((34.0 * ca + px * 0.4, 34.0 * sa + py * 0.4, -22.0))
        v3 = bm.verts.new((22.0 * ca + px, 22.0 * sa + py, -16.0))
        v4 = bm.verts.new((16.0 * ca - px, 16.0 * sa - py, -46.0))
        v5 = bm.verts.new((40.0 * ca - px * 0.4, 40.0 * sa - py * 0.4, -42.0))
        v6 = bm.verts.new((34.0 * ca - px * 0.4, 34.0 * sa - py * 0.4, -22.0))
        v7 = bm.verts.new((22.0 * ca - px, 22.0 * sa - py, -16.0))
        bm.faces.new((v0, v1, v2, v3))
        bm.faces.new((v7, v6, v5, v4))
        bm.faces.new((v0, v4, v5, v1))
        bm.faces.new((v1, v5, v6, v2))
        bm.faces.new((v2, v6, v7, v3))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    add_cylindrical_uv(mesh)

    obj = bpy.data.objects.new('CannonShell', mesh)
    bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(dest / 'SM_Source.fbx'),
        use_selection=True,
        object_types={'MESH'},
        add_leaf_bones=False,
        bake_anim=False,
        axis_forward='-X',
        axis_up='Z',
    )

    # Generate 256x256 textures for CannonShell
    w = h = 256
    base = bytearray(w * h * 3)
    norm = bytearray(w * h * 3)
    rough = bytearray(w * h * 3)
    metal = bytearray(w * h * 3)
    for y in range(h):
        v = 1.0 - (y / (h - 1))  # v=1 at nose, v=0 at tail
        for x in range(w):
            u = x / (w - 1)
            idx = (y * w + x) * 3
            stripe = 0.5 + 0.5 * math.sin(u * 16.0 * math.pi)
            ring_groove = (abs(v - 0.42) < 0.035) or (abs(v - 0.63) < 0.035)
            if v > 0.72:
                # Superheated glowing plasma warhead tip
                t = (v - 0.72) / 0.28
                r = clamp01(1.0)
                g = clamp01(0.68 + 0.30 * t + 0.05 * stripe)
                b = clamp01(0.18 + 0.55 * (t * t))
                ro, me = 45, 60
            elif ring_groove:
                # Glowing plasma cooling rings
                r, g, b = 1.0, 0.62, 0.14
                ro, me = 40, 40
            elif v < 0.28:
                # Incandescent plasma tracer tail & fins
                t = 1.0 - (v / 0.28)
                r = 1.0
                g = clamp01(0.48 + 0.42 * t)
                b = clamp01(0.10 + 0.35 * (t * t))
                ro, me = 50, 50
            else:
                # Armored sabot gunmetal/brass bands with glowing panel lines
                band = math.sin(v * 28.0 * math.pi)
                panel = 1.0 if abs(math.sin(u * 8.0 * math.pi)) > 0.92 else 0.0
                r = clamp01(0.46 + 0.14 * band + 0.45 * panel)
                g = clamp01(0.40 + 0.10 * band + 0.28 * panel)
                b = clamp01(0.34 + 0.08 * band + 0.05 * panel)
                ro, me = 95, 190
            base[idx:idx + 3] = (int(r * 255), int(g * 255), int(b * 255))
            nx = int(128 + 28 * math.cos(u * 16.0 * math.pi))
            ny = int(128 + (45 if ring_groove else 12 * math.sin(v * 24.0 * math.pi)))
            norm[idx:idx + 3] = (nx, ny, 235)
            rough[idx:idx + 3] = (ro, ro, ro)
            metal[idx:idx + 3] = (me, me, me)

    write_png(dest / 'T_Base.png', w, h, base)
    write_png(dest / 'T_Normal.png', w, h, norm)
    write_png(dest / 'T_Roughness.png', w, h, rough)
    write_png(dest / 'T_Metallic.png', w, h, metal)


def build_laser_beam(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    clear_scene()
    mesh = bpy.data.meshes.new('LaserBeamMesh')
    bm = bmesh.new()

    # Multi-layered fluted energy beam from Z=-50 to Z=+50
    z_steps = [-50.0, -46.0, -35.0, -18.0, 0.0, 18.0, 35.0, 46.0, 50.0]
    r_scales = [0.35, 0.82, 0.96, 1.0, 1.04, 1.0, 0.96, 0.82, 0.35]
    segs = 16
    rings = []
    for z, sc in zip(z_steps, r_scales):
        ring = []
        for i in range(segs):
            ang = 2.0 * math.pi * i / segs
            flute = 1.0 + 0.22 * math.cos(4.0 * ang + (z / 50.0) * math.pi)
            rad = 38.0 * sc * flute
            ring.append(bm.verts.new((rad * math.cos(ang), rad * math.sin(ang), z)))
        rings.append(ring)

    for r_idx in range(len(rings) - 1):
        r0, r1 = rings[r_idx], rings[r_idx + 1]
        for i in range(segs):
            ni = (i + 1) % segs
            bm.faces.new((r0[i], r0[ni], r1[ni], r1[i]))

    # Crossed plasma energy fins along the beam for rich silhouette from all angles
    for plane in range(2):
        ang = plane * (math.pi * 0.5) + math.pi * 0.25
        ca, sa = math.cos(ang), math.sin(ang)
        nx, ny = -sa * 2.5, ca * 2.5
        for r_idx in range(len(z_steps) - 1):
            z0, z1 = z_steps[r_idx], z_steps[r_idx + 1]
            w0, w1 = 48.0 * r_scales[r_idx], 48.0 * r_scales[r_idx + 1]
            v0 = bm.verts.new((-w0 * ca + nx, -w0 * sa + ny, z0))
            v1 = bm.verts.new((w0 * ca + nx, w0 * sa + ny, z0))
            v2 = bm.verts.new((w1 * ca + nx, w1 * sa + ny, z1))
            v3 = bm.verts.new((-w1 * ca + nx, -w1 * sa + ny, z1))
            v4 = bm.verts.new((-w0 * ca - nx, -w0 * sa - ny, z0))
            v5 = bm.verts.new((w0 * ca - nx, w0 * sa - ny, z0))
            v6 = bm.verts.new((w1 * ca - nx, w1 * sa - ny, z1))
            v7 = bm.verts.new((-w1 * ca - nx, -w1 * sa - ny, z1))
            bm.faces.new((v0, v1, v2, v3))
            bm.faces.new((v7, v6, v5, v4))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    add_cylindrical_uv(mesh)

    obj = bpy.data.objects.new('LaserBeam', mesh)
    bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(dest / 'SM_Source.fbx'),
        use_selection=True,
        object_types={'MESH'},
        add_leaf_bones=False,
        bake_anim=False,
        axis_forward='-X',
        axis_up='Z',
    )

    w = h = 256
    friendly = bytearray(w * h * 3)
    enemy = bytearray(w * h * 3)
    norm = bytearray(w * h * 3)
    rough = bytearray(w * h * 3)
    metal = bytearray(w * h * 3)
    for y in range(h):
        v = y / (h - 1)
        for x in range(w):
            u = x / (w - 1)
            idx = (y * w + x) * 3
            # Helical plasma wave + intense core streaks
            helix = 0.5 + 0.5 * math.sin((u * 8.0 + v * 6.0) * math.pi)
            pulse = 0.5 + 0.5 * math.cos(v * 18.0 * math.pi)
            core = math.pow(0.5 + 0.5 * math.cos(u * 4.0 * math.pi), 2.0)
            energy = clamp01(0.42 + 0.35 * core + 0.15 * helix + 0.08 * pulse)

            # Friendly: cyan-blue plasma with white-hot core filaments
            fr = clamp01(0.18 + 0.78 * (energy ** 2.2))
            fg = clamp01(0.72 + 0.28 * energy)
            fb = 1.0
            friendly[idx:idx + 3] = (int(fr * 255), int(fg * 255), int(fb * 255))

            # Enemy: crimson-orange high-energy laser with white-gold core filaments
            er = 1.0
            eg = clamp01(0.22 + 0.68 * (energy ** 1.8))
            eb = clamp01(0.10 + 0.52 * (energy ** 2.6))
            enemy[idx:idx + 3] = (int(er * 255), int(eg * 255), int(eb * 255))

            nx = int(128 + 35 * math.cos((u * 8.0 + v * 6.0) * math.pi))
            ny = int(128 + 25 * math.sin(v * 18.0 * math.pi))
            norm[idx:idx + 3] = (nx, ny, 235)
            rough[idx:idx + 3] = (30, 30, 30)
            metal[idx:idx + 3] = (40, 40, 40)

    write_png(dest / 'T_FriendlyBase.png', w, h, friendly)
    write_png(dest / 'T_EnemyBase.png', w, h, enemy)
    write_png(dest / 'T_Normal.png', w, h, norm)
    write_png(dest / 'T_Roughness.png', w, h, rough)
    write_png(dest / 'T_Metallic.png', w, h, metal)


def build_impact_burst(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    clear_scene()
    mesh = bpy.data.meshes.new('ImpactBurstMesh')
    bm = bmesh.new()

    # Core spiked plasma burst sphere + 2 angled shockwave rings
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=38.0)
    for idx, v in enumerate(bm.verts):
        ang_factor = 1.0 + 0.24 * math.sin(idx * 2.7) * math.cos(idx * 1.9)
        v.co *= ang_factor

    # Add two shockwave rings (equatorial and tilted)
    for tilt in (0.28, -0.32):
        ct, st = math.cos(tilt), math.sin(tilt)
        segs = 24
        r_in, r_out = 30.0, 50.0
        ring_in, ring_out_top, ring_out_bot = [], [], []
        for i in range(segs):
            a = 2.0 * math.pi * i / segs
            ca, sa = math.cos(a), math.sin(a)
            spike = 1.0 + 0.08 * math.cos(6.0 * a)

            def rot_x(x, y, z):
                return (x, y * ct - z * st, y * st + z * ct)

            ring_in.append(bm.verts.new(rot_x(r_in * ca, r_in * sa, 0.0)))
            ring_out_top.append(bm.verts.new(rot_x(r_out * spike * ca, r_out * spike * sa, 3.2)))
            ring_out_bot.append(bm.verts.new(rot_x(r_out * spike * ca, r_out * spike * sa, -3.2)))
        for i in range(segs):
            ni = (i + 1) % segs
            bm.faces.new((ring_in[i], ring_out_top[i], ring_out_top[ni], ring_in[ni]))
            bm.faces.new((ring_in[ni], ring_out_bot[ni], ring_out_bot[i], ring_in[i]))
            bm.faces.new((ring_out_top[i], ring_out_bot[i], ring_out_bot[ni], ring_out_top[ni]))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    add_spherical_uv(mesh)

    obj = bpy.data.objects.new('ImpactBurst', mesh)
    bpy.context.collection.objects.link(obj)
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(dest / 'SM_Source.fbx'),
        use_selection=True,
        object_types={'MESH'},
        add_leaf_bones=False,
        bake_anim=False,
        axis_forward='-X',
        axis_up='Z',
    )

    w = h = 256
    shield_tex = bytearray(w * h * 3)
    hull_tex = bytearray(w * h * 3)
    plume_tex = bytearray(w * h * 3)
    norm = bytearray(w * h * 3)
    rough = bytearray(w * h * 3)
    metal = bytearray(w * h * 3)

    for y in range(h):
        v = y / (h - 1)
        for x in range(w):
            u = x / (w - 1)
            idx = (y * w + x) * 3
            burst = 0.5 + 0.5 * math.sin(u * 14.0 * math.pi) * math.cos(v * 10.0 * math.pi)
            eq_glow = math.exp(-((v - 0.5) * 4.2) ** 2)
            energy = clamp01(0.38 + 0.38 * burst + 0.24 * eq_glow)

            # Shield Impact: Electric cyan-blue discharge ripples
            sr = clamp01(0.20 + 0.72 * (energy ** 2.0))
            sg = clamp01(0.74 + 0.26 * energy)
            sb = 1.0
            shield_tex[idx:idx + 3] = (int(sr * 255), int(sg * 255), int(sb * 255))

            # Hull Impact: Fiery orange-gold detonation & sparks
            hr = 1.0
            hg = clamp01(0.32 + 0.62 * (energy ** 1.5))
            hb = clamp01(0.08 + 0.55 * (energy ** 2.8))
            hull_tex[idx:idx + 3] = (int(hr * 255), int(hg * 255), int(hb * 255))

            # Missile Plume: White-hot core to bright amber exhaust
            pr = 1.0
            pg = clamp01(0.52 + 0.45 * energy)
            pb = clamp01(0.18 + 0.65 * (energy ** 2.2))
            plume_tex[idx:idx + 3] = (int(pr * 255), int(pg * 255), int(pb * 255))

            nx = int(128 + 30 * math.cos(u * 14.0 * math.pi))
            ny = int(128 + 30 * math.sin(v * 10.0 * math.pi))
            norm[idx:idx + 3] = (nx, ny, 235)
            rough[idx:idx + 3] = (35, 35, 35)
            metal[idx:idx + 3] = (35, 35, 35)

    write_png(dest / 'T_ShieldBase.png', w, h, shield_tex)
    write_png(dest / 'T_HullBase.png', w, h, hull_tex)
    write_png(dest / 'T_PlumeBase.png', w, h, plume_tex)
    write_png(dest / 'T_Normal.png', w, h, norm)
    write_png(dest / 'T_Roughness.png', w, h, rough)
    write_png(dest / 'T_Metallic.png', w, h, metal)


build_cannon_shell(out_root / 'CannonShell')
build_laser_beam(out_root / 'LaserBeam')
build_impact_burst(out_root / 'ImpactBurst')
print('OV_COMBAT_EFFECT_ASSETS_GENERATED', out_root)
