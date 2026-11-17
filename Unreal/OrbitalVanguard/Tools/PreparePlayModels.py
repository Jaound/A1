"""Blender 5.2 배치: 원본 FBX 읽기·연결 성분 조사, 초계함 임시 경량본 작성.

사용: blender --background --python PreparePlayModels.py -- --config <PrepareConfig.json>
원본은 쓰지 않는다. 출력 폴더가 이미 있으면 중단하여 기존 작업을 보호한다.
관절 분리를 추정하지 않으며 원본 무장은 조사와 정지 미리보기만 만든다.
"""
import argparse
from array import array
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import shutil
import sys
import time

import bpy
from mathutils import Matrix, Vector


def sha(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def components(mesh):
    count = len(mesh.vertices)
    parents = array("i", range(count))
    sizes = array("i", [1]) * count

    def find(index):
        while parents[index] != index:
            parents[index] = parents[parents[index]]
            index = parents[index]
        return index

    for edge in mesh.edges:
        a, b = (find(i) for i in edge.vertices)
        if a == b:
            continue
        if sizes[a] < sizes[b]:
            a, b = b, a
        parents[b] = a
        sizes[a] += sizes[b]
    totals = Counter(find(i) for i in range(count))
    return {"connected_vertex_components": len(totals), "largest_component_vertices": sorted(totals.values(), reverse=True)[:12]}


def object_report(obj):
    mesh = obj.data
    result = {"name": obj.name, "vertices": len(mesh.vertices), "edges": len(mesh.edges),
              "polygons": len(mesh.polygons), "uv_layers": [u.name for u in mesh.uv_layers],
              "dimensions_blender": list(obj.dimensions), "materials": [m.name for m in mesh.materials if m]}
    result.update(components(mesh))
    return result


def preview(destination):
    scene = bpy.context.scene
    meshes = [o for o in scene.objects if o.type == "MESH"]
    points = [o.matrix_world @ Vector(v) for o in meshes for v in o.bound_box]
    center = sum(points, Vector()) / max(1, len(points))
    radius = max((v - center).length for v in points)
    camera = bpy.data.cameras.new("검사용 카메라")
    obj = bpy.data.objects.new("검사용 카메라", camera)
    scene.collection.objects.link(obj)
    obj.location = center + Vector((1.3, -1.5, 1.15)).normalized() * radius * 3.2
    obj.rotation_euler = (center - obj.location).to_track_quat("-Z", "Y").to_euler()
    camera.type = "ORTHO"
    camera.ortho_scale = radius * 2.35
    scene.camera = obj
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_shadows = True
    scene.display.shading.show_cavity = True
    scene.display.shading.background_type = "WORLD"
    if not scene.world:
        scene.world = bpy.data.worlds.new("검사용 배경")
    scene.world.color = (.025, .035, .05)
    scene.render.resolution_x = 800
    scene.render.resolution_y = 600
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(destination)
    bpy.ops.render.render(write_still=True)


args = argparse.ArgumentParser()
args.add_argument("--config", required=True)
options = args.parse_args(sys.argv[sys.argv.index("--") + 1:])
config = json.loads(Path(options.config).read_text(encoding="utf-8"))
root = Path(config["asset_root"])
out_root = root / "Prepared" / config["tag"]
out_root.mkdir(parents=True, exist_ok=False)
report_path = root / "Reports" / (config["tag"] + "_Blender.json")
report = {"blender_version": bpy.app.version_string, "source_modified": False,
          "scope": "연결 성분·UV·재질 조사. 초계함 경량 시험본. 원본 포탑 관절 분리는 미수행.", "models": []}
for entry in config["models"]:
    source = Path(entry["source_fbx"])
    row = dict(entry)
    row["source_bytes"] = source.stat().st_size
    row["source_sha256_before"] = sha(source)
    row["status"] = "검사 중"
    report["models"].append(row)
    try:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(source), use_image_search=True)
        objects = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        row["original_meshes"] = [object_report(o) for o in objects]
        row["images"] = [{"name": image.name, "path": bpy.path.abspath(image.filepath),
                          "exists": Path(bpy.path.abspath(image.filepath)).is_file(),
                          "size": list(image.size), "packed": bool(image.packed_file)}
                         for image in bpy.data.images if image.source == "FILE"]
        model_out = out_root / entry["id"]
        model_out.mkdir()
        if entry.get("prepare"):
            bpy.ops.object.select_all(action="DESELECT")
            for obj in objects:
                obj.select_set(True)
            bpy.context.view_layer.objects.active = objects[0]
            if len(objects) > 1:
                bpy.ops.object.join()
            hull = bpy.context.active_object
            hull.name = "OV_PC14_Preview"
            bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
            total_faces = len(hull.data.polygons)
            row["decimation"] = {"method": "Blender COLLAPSE", "input_faces": total_faces,
                                 "target_faces": 10000, "uv_preservation": "UV 경계 구분을 유지하는 COLLAPSE; 최종 텍스처 품질 승인 아님"}
            modifier = hull.modifiers.new("PlayUI_10000_faces", "DECIMATE")
            modifier.ratio = min(1.0, 10000 / max(1, total_faces))
            modifier.use_collapse_triangulate = True
            modifier.delimit = {"UV", "MATERIAL", "SEAM"}
            bpy.ops.object.modifier_apply(modifier=modifier.name)
            corners = [Vector(v) for v in hull.bound_box]
            minimum = Vector(tuple(min(v[i] for v in corners) for i in range(3)))
            maximum = Vector(tuple(max(v[i] for v in corners) for i in range(3)))
            center = (minimum + maximum) / 2
            extent = maximum - minimum
            major = max(range(3), key=lambda i: extent[i])
            rotate = Matrix.Identity(4)
            if major == 1:
                rotate = Matrix.Rotation(-math.pi / 2, 4, "Z")
            elif major == 2:
                rotate = Matrix.Rotation(math.pi / 2, 4, "Y")
            scale = 3.5 / max(extent)
            transform = Matrix.Scale(scale, 4) @ rotate @ Matrix.Translation(-center)
            hull.data.transform(transform)
            hull.location = Vector()
            hull.data.update()
            bpy.context.view_layer.update()
            bpy.context.scene.unit_settings.system = "METRIC"
            bpy.context.scene.unit_settings.scale_length = 1.0
            row["transform"] = {"source_major_axis": "XYZ"[major], "source_bbox": [list(minimum), list(maximum)],
                                "uniform_scale": scale, "prepared_longest_meters": 3.5,
                                "ue_expected_longest_cm": 350, "front_facing": "장축 X 정렬, 원본 앞뒤 방향은 시각 확인 필요"}
            textures = model_out / "Textures"
            textures.mkdir()
            for image in bpy.data.images:
                if image.source != "FILE":
                    continue
                file = Path(bpy.path.abspath(image.filepath))
                target = textures / (image.name if image.name.lower().endswith(".png") else image.name + ".png")
                if image.packed_file:
                    image.filepath_raw = str(target)
                    image.file_format = "PNG"
                    image.save()
                elif file.is_file():
                    if not target.exists():
                        shutil.copy2(file, target)
                    image.filepath = str(target)
            row["prepared_mesh"] = object_report(hull)
            fbx = model_out / "OV_PC14_Preview.fbx"
            blend = model_out / "OV_PC14_Preview.blend"
            bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={"MESH"},
                                     apply_unit_scale=True, global_scale=1.0, axis_forward="-Y", axis_up="Z",
                                     add_leaf_bones=False, bake_anim=False, path_mode="COPY", embed_textures=False)
            bpy.ops.wm.save_as_mainfile(filepath=str(blend))
            row["prepared_fbx"] = str(fbx)
            row["prepared_fbx_sha256"] = sha(fbx)
            row["prepared_blend"] = str(blend)
            row["status"] = "경량 초계함 생성 · UE 임포트 및 외형 승인 별도"
        else:
            largest = max((m["largest_component_vertices"][0] for m in row["original_meshes"]), default=0)
            vertices = sum(m["vertices"] for m in row["original_meshes"])
            row["largest_component_fraction"] = largest / max(1, vertices)
            row["articulation"] = "받침대·회전부·포신의 의미 구분과 피벗을 안전하게 자동 추정할 수 없어 원본 분리 미수행. 임시 관절 포탑 사용."
            row["status"] = "원본 검사 완료 · 관절 분리 미수행"
        image_path = model_out / "Inspection.png"
        preview(image_path)
        row["inspection_png"] = str(image_path)
    except Exception as error:
        import traceback
        row["status"] = "실패"
        row["error"] = str(error)
        row["traceback"] = traceback.format_exc()
    finally:
        row["source_sha256_after"] = sha(source)
        row["source_unchanged"] = row["source_sha256_before"] == row["source_sha256_after"]
        # Atomic replacement avoids readers observing a half-written JSON during long import runs.
        temporary_report = report_path.with_suffix(".writing.json")
        temporary_report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        for attempt in range(4):
            try:
                temporary_report.replace(report_path)
                break
            except OSError:
                if attempt == 3:
                    raise
                time.sleep(.5)
        print("OVMODEL", entry["id"], row["status"], flush=True)
