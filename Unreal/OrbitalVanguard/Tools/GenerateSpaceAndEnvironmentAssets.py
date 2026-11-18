"""Blender 5.2 script to generate 3D meshes (.fbx) and textures (.png) for SpaceSkybox, Asteroid, BlackHole, and HazardZone."""
import math
from pathlib import Path
import random
import struct
import zlib
import bpy
import bmesh

out_root = Path.home() / 'Documents/OrbitalVanguardAssets/Prepared/SpaceEnvironment_v1'
out_root.mkdir(parents=True, exist_ok=True)


def write_png(path: Path, width: int, height: int, rgb_bytes: bytes):
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


def add_spherical_uv_and_scale(mesh, scale=0.01):
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
        vert.co *= scale


def build_space_skybox(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    clear_scene()
    mesh = bpy.data.meshes.new('SpaceSkyboxMesh')
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=48, v_segments=24, radius=50.0)
    # Flip normals inward so skybox is visible from inside (and material will also be two-sided)
    for f in bm.faces:
        f.normal_flip()
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    for f in bm.faces:
        f.normal_flip()
    bm.to_mesh(mesh)
    bm.free()
    add_spherical_uv_and_scale(mesh, 0.01)

    obj = bpy.data.objects.new('SpaceSkybox', mesh)
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

    # Generate 512x256 rich cosmic nebula + galactic band + starfield panorama
    w, h = 512, 256
    base = bytearray(w * h * 3)
    norm = bytearray(w * h * 3)
    rough = bytearray(w * h * 3)
    metal = bytearray(w * h * 3)

    for y in range(h):
        v = y / (h - 1)
        lat = (v - 0.5) * math.pi
        for x in range(w):
            u = x / (w - 1)
            lon = (u - 0.5) * 2.0 * math.pi
            idx = (y * w + x) * 3

            # Tilted galactic plane
            gal_lat = lat + 0.32 * math.sin(lon * 1.0 + 0.6) + 0.08 * math.cos(lon * 3.0)
            band = math.exp(-((gal_lat / 0.28) ** 2))
            core_glow = math.exp(-((lon - 0.4) ** 2) / 0.8) * band

            # Multi-octave procedural nebula waves
            n1 = 0.5 + 0.5 * math.sin(lon * 3.0 + math.cos(lat * 4.0))
            n2 = 0.5 + 0.5 * math.cos(lon * 7.0 - math.sin(lat * 5.0) * 1.5)
            n3 = 0.5 + 0.5 * math.sin((lon + lat) * 11.0)
            neb_cyan = band * (n1 * 0.65 + n2 * 0.35)
            neb_purple = math.exp(-(((gal_lat - 0.14) / 0.36) ** 2)) * (n2 * 0.6 + n3 * 0.4)
            neb_gold = core_glow * (0.5 + 0.5 * n3)

            # Deep space base color + nebula layers
            r = 0.022 + 0.08 * neb_cyan + 0.22 * neb_purple + 0.42 * neb_gold
            g = 0.035 + 0.26 * neb_cyan + 0.09 * neb_purple + 0.28 * neb_gold
            b = 0.075 + 0.42 * neb_cyan + 0.32 * neb_purple + 0.12 * neb_gold

            # Deterministic star hash
            hval = ((x * 73856093) ^ (y * 19349663)) & 0xFFFF
            if hval < 190:
                bright = (190 - hval) / 190.0
                if hval % 5 == 0:
                    r += 0.95 * bright
                    g += 0.78 * bright
                    b += 0.52 * bright
                elif hval % 3 == 0:
                    r += 0.65 * bright
                    g += 0.88 * bright
                    b += 1.0 * bright
                else:
                    r += 0.92 * bright
                    g += 0.95 * bright
                    b += 1.0 * bright

            base[idx:idx + 3] = (int(clamp01(r) * 255), int(clamp01(g) * 255), int(clamp01(b) * 255))
            norm[idx:idx + 3] = (128, 128, 255)
            rough[idx:idx + 3] = (240, 240, 240)
            metal[idx:idx + 3] = (0, 0, 0)

    write_png(dest / 'T_Base.png', w, h, base)
    write_png(dest / 'T_Normal.png', w, h, norm)
    write_png(dest / 'T_Roughness.png', w, h, rough)
    write_png(dest / 'T_Metallic.png', w, h, metal)


def build_asteroid(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    clear_scene()
    mesh = bpy.data.meshes.new('AsteroidMesh')
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=3, radius=48.0)

    # Displace vertices deterministically into craggy asteroid with craters and ridges
    for i, v in enumerate(bm.verts):
        nx, ny, nz = v.co.normalized()
        ridge = 0.14 * math.sin(nx * 4.2 + ny * 3.1) * math.cos(nz * 3.7 - nx * 2.3)
        crater = -0.10 * max(0.0, math.sin(nx * 7.5) * math.cos(ny * 7.5) * math.sin(nz * 7.5))
        oblate = 1.0 + 0.12 * nx - 0.08 * (nz * nz)
        v.co *= (oblate + ridge + crater)

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    add_spherical_uv_and_scale(mesh, 0.01)

    obj = bpy.data.objects.new('Asteroid', mesh)
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
    base = bytearray(w * h * 3)
    norm = bytearray(w * h * 3)
    rough = bytearray(w * h * 3)
    metal = bytearray(w * h * 3)
    for y in range(h):
        v = y / (h - 1)
        for x in range(w):
            u = x / (w - 1)
            idx = (y * w + x) * 3
            n1 = math.sin(u * 18.0 * math.pi) * math.cos(v * 14.0 * math.pi)
            n2 = math.cos((u * 32.0 + v * 22.0) * math.pi)
            vein = 1.0 if abs(math.sin((u * 9.0 + 0.25 * math.sin(v * 6.0 * math.pi)) * math.pi)) > 0.965 else 0.0
            rock = 0.28 + 0.09 * n1 + 0.05 * n2
            r = clamp01(rock * 1.05 + 0.25 * vein)
            g = clamp01(rock * 0.98 + 0.55 * vein)
            b = clamp01(rock * 0.90 + 0.75 * vein)
            base[idx:idx + 3] = (int(r * 255), int(g * 255), int(b * 255))
            nx = int(128 + 42 * n1)
            ny = int(128 + 42 * n2)
            norm[idx:idx + 3] = (nx, ny, 225)
            rough[idx:idx + 3] = (210 if vein == 0 else 70,) * 3
            metal[idx:idx + 3] = (35 if vein == 0 else 180,) * 3

    write_png(dest / 'T_Base.png', w, h, base)
    write_png(dest / 'T_Normal.png', w, h, norm)
    write_png(dest / 'T_Roughness.png', w, h, rough)
    write_png(dest / 'T_Metallic.png', w, h, metal)


def build_black_hole(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    clear_scene()
    mesh = bpy.data.meshes.new('BlackHoleMesh')
    bm = bmesh.new()

    # Event horizon sphere (radius 20) + wide swirling accretion disk (radius 22..50) + lensing ring
    bmesh.ops.create_uvsphere(bm, u_segments=28, v_segments=16, radius=20.0)

    segs = 36
    radii = [21.0, 28.0, 38.0, 50.0]
    heights = [3.5, 2.2, 1.2, 0.3]
    for side in (1.0, -1.0):
        rings = []
        for rad, hz in zip(radii, heights):
            ring = []
            for i in range(segs):
                a = 2.0 * math.pi * i / segs
                ring.append(bm.verts.new((rad * math.cos(a), rad * math.sin(a), side * hz)))
            rings.append(ring)
        for r_idx in range(len(rings) - 1):
            r0, r1 = rings[r_idx], rings[r_idx + 1]
            for i in range(segs):
                ni = (i + 1) % segs
                if side > 0:
                    bm.faces.new((r0[i], r1[i], r1[ni], r0[ni]))
                else:
                    bm.faces.new((r0[ni], r1[ni], r1[i], r0[i]))

    # Vertical gravitational lensing halo ring
    halo_in, halo_out = [], []
    for i in range(segs):
        a = 2.0 * math.pi * i / segs
        ca, sa = math.cos(a), math.sin(a)
        halo_in.append(bm.verts.new((21.0 * ca, 2.0, 21.0 * sa)))
        halo_out.append(bm.verts.new((27.0 * ca, 0.0, 27.0 * sa)))
    for i in range(segs):
        ni = (i + 1) % segs
        bm.faces.new((halo_in[i], halo_out[i], halo_out[ni], halo_in[ni]))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    add_spherical_uv_and_scale(mesh, 0.01)

    obj = bpy.data.objects.new('BlackHole', mesh)
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
    base = bytearray(w * h * 3)
    norm = bytearray(w * h * 3)
    rough = bytearray(w * h * 3)
    metal = bytearray(w * h * 3)
    for y in range(h):
        v = y / (h - 1)
        for x in range(w):
            u = x / (w - 1)
            idx = (y * w + x) * 3
            eq = math.exp(-(((v - 0.5) / 0.16) ** 2))
            swirl = 0.5 + 0.5 * math.sin((u * 10.0 + v * 8.0) * math.pi)
            ring_glow = clamp01(eq * (0.55 + 0.45 * swirl))
            # Dark event horizon away from equator, blazing violet-amber accretion disk near equator
            r = clamp01(0.06 + 0.94 * ring_glow)
            g = clamp01(0.03 + 0.52 * (ring_glow ** 1.4))
            b = clamp01(0.12 + 0.88 * (0.6 * ring_glow + 0.4 * swirl * eq))
            base[idx:idx + 3] = (int(r * 255), int(g * 255), int(b * 255))
            norm[idx:idx + 3] = (128, 128, 255)
            rough[idx:idx + 3] = (40, 40, 40)
            metal[idx:idx + 3] = (20, 20, 20)

    write_png(dest / 'T_Base.png', w, h, base)
    write_png(dest / 'T_Normal.png', w, h, norm)
    write_png(dest / 'T_Roughness.png', w, h, rough)
    write_png(dest / 'T_Metallic.png', w, h, metal)


def build_hazard_zone(dest: Path):
    dest.mkdir(parents=True, exist_ok=True)
    clear_scene()
    mesh = bpy.data.meshes.new('HazardZoneMesh')
    bm = bmesh.new()
    # Concentric plasma hazard rings + boundary shell (radius 50 cm)
    segs = 36
    for rad, z_off, thick in [(49.0, 0.0, 3.5), (36.0, 2.0, 2.5), (22.0, -2.0, 2.0)]:
        r_top, r_bot = [], []
        for i in range(segs):
            a = 2.0 * math.pi * i / segs
            wave = 1.0 + 0.04 * math.sin(6.0 * a)
            r_top.append(bm.verts.new((rad * wave * math.cos(a), rad * wave * math.sin(a), z_off + thick)))
            r_bot.append(bm.verts.new((rad * wave * math.cos(a), rad * wave * math.sin(a), z_off - thick)))
        for i in range(segs):
            ni = (i + 1) % segs
            bm.faces.new((r_bot[i], r_top[i], r_top[ni], r_bot[ni]))

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    add_spherical_uv_and_scale(mesh, 0.01)

    obj = bpy.data.objects.new('HazardZone', mesh)
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
    rad_tex = bytearray(w * h * 3)
    flare_tex = bytearray(w * h * 3)
    norm = bytearray(w * h * 3)
    rough = bytearray(w * h * 3)
    metal = bytearray(w * h * 3)
    for y in range(h):
        v = y / (h - 1)
        for x in range(w):
            u = x / (w - 1)
            idx = (y * w + x) * 3
            pulse = 0.5 + 0.5 * math.sin(u * 12.0 * math.pi) * math.cos(v * 8.0 * math.pi)
            # Radiation Zone: Ionizing emerald-cyan glow
            rad_tex[idx:idx + 3] = (
                int(clamp01(0.14 + 0.35 * pulse) * 255),
                int(clamp01(0.75 + 0.25 * pulse) * 255),
                int(clamp01(0.42 + 0.45 * pulse) * 255),
            )
            # Solar Flare: Blazing solar gold-orange corona wave
            flare_tex[idx:idx + 3] = (
                255,
                int(clamp01(0.48 + 0.48 * pulse) * 255),
                int(clamp01(0.12 + 0.55 * (pulse * pulse)) * 255),
            )
            norm[idx:idx + 3] = (128, 128, 255)
            rough[idx:idx + 3] = (50, 50, 50)
            metal[idx:idx + 3] = (20, 20, 20)

    write_png(dest / 'T_RadiationBase.png', w, h, rad_tex)
    write_png(dest / 'T_SolarFlareBase.png', w, h, flare_tex)
    write_png(dest / 'T_Normal.png', w, h, norm)
    write_png(dest / 'T_Roughness.png', w, h, rough)
    write_png(dest / 'T_Metallic.png', w, h, metal)


build_space_skybox(out_root / 'SpaceSkybox')
build_asteroid(out_root / 'Asteroid')
build_black_hole(out_root / 'BlackHole')
build_hazard_zone(out_root / 'HazardZone')
print('OV_SPACE_ENVIRONMENT_ASSETS_GENERATED', out_root)
