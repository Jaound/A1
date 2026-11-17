"""Prepared 초계함을 정상 UE StaticMesh 임포트 경로로 생성한다. 기존 에셋 덮어쓰기 금지.

텍스처/재질과 자동 충돌은 반입하지 않는다. 원본 PNG·FBX는 로컬 자산 폴더에 보존한다.
명령: UnrealEditor.exe OrbitalVanguard.uproject -ExecutePythonScript=<이 파일> -unattended -nullrhi
기존 에셋 검사만: 환경 변수 OV_VALIDATE_IMPORTED_ONLY=1. 별도 실행한 에디터는 종료한다.
"""
from pathlib import Path
import hashlib
import json
import os
import traceback
import unreal

source = Path.home() / "Documents/OrbitalVanguardAssets/Prepared/PlayUI_v1_20261117b/corvette_pc14/OV_PC14_Preview.fbx"
asset_path = "/Game/Presentation/Ships/SM_PC14"
output = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())) / "Validation/PlayUI/ImportedCorvette.json"
report = {"passed": False, "scope": "UE 정적 메시 임포트. 실제 전장 적용/조작 검증은 별도.",
          "source": str(source), "asset": asset_path, "source_modified": False,
          "textures_imported": False, "materials_imported": False, "auto_collision": False,
          "material_policy": "기존 진영 임시 재질 사용. 원본 텍스처는 Prepared에 보존하나 UE에는 미적용.",
          "checks": [], "errors": []}
validate_only = os.environ.get("OV_VALIDATE_IMPORTED_ONLY") == "1"
report["validation_only"] = validate_only


def check(identifier, condition, detail=""):
    report["checks"].append({"id": identifier, "passed": bool(condition), "detail": str(detail)})
    if not condition:
        raise AssertionError(identifier + ": " + str(detail))


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


try:
    check("MODEL-SOURCE-BYTES", source.is_file() and source.stat().st_size > 1000)
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    check("MODEL-EDITOR-CONTEXT", editor is not None, "StaticMeshEditorSubsystem이 초기화된 별도 에디터로 실행하세요")
    report["source_sha256"] = digest(source)
    check("MODEL-NO-OVERWRITE", validate_only or not unreal.EditorAssetLibrary.does_asset_exist(asset_path))
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    settings = options.get_editor_property("static_mesh_import_data")
    settings.set_editor_property("combine_meshes", True)
    settings.set_editor_property("auto_generate_collision", False)
    settings.set_editor_property("generate_lightmap_u_vs", False)
    settings.set_editor_property("convert_scene", True)
    settings.set_editor_property("convert_scene_unit", True)
    settings.set_editor_property("import_uniform_scale", 1.0)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", "/Game/Presentation/Ships")
    task.set_editor_property("destination_name", "SM_PC14")
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.FbxFactory())
    task.set_editor_property("options", options)
    if not validate_only:
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = [asset_path + ".SM_PC14"] if validate_only else task.get_editor_property("imported_object_paths")
    report["imported_paths"] = list(paths)
    check("MODEL-ONE-ASSET", len(paths) == 1, paths)
    mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    check("MODEL-STATIC-MESH", isinstance(mesh, unreal.StaticMesh))
    bounds = mesh.get_bounding_box()
    low, high = bounds.min, bounds.max
    extent = [float(getattr(high, axis) - getattr(low, axis)) for axis in ("x", "y", "z")]
    center = [float((getattr(high, axis) + getattr(low, axis)) / 2) for axis in ("x", "y", "z")]
    report["bounds_size_cm"] = extent
    report["bounds_center_cm"] = center
    report["lods"] = editor.get_lod_count(mesh)
    report["triangles_lod0"] = mesh.get_num_triangles(0)
    report["vertices_lod0"] = mesh.get_num_vertices(0)
    report["uv_channels"] = editor.get_num_uv_channels(mesh, 0)
    report["simple_collisions"] = editor.get_simple_collision_count(mesh)
    check("MODEL-TRIANGLE-BUDGET", 1000 <= report["triangles_lod0"] <= 12000, report["triangles_lod0"])
    check("MODEL-UV-PRESERVED", report["uv_channels"] >= 1)
    check("MODEL-X-LONGEST", extent[0] > extent[1] and extent[0] > extent[2], extent)
    check("MODEL-CENTIMETER-SCALE", abs(extent[0] - 350) < 1, extent)
    check("MODEL-CENTERED", max(abs(v) for v in center) < 1, center)
    check("MODEL-NO-AUTO-COLLISION", report["simple_collisions"] == 0)
    check("MODEL-SAVE", unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False))
    asset_file = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())) / "Presentation/Ships/SM_PC14.uasset"
    report["uasset"] = str(asset_file)
    report["uasset_bytes"] = asset_file.stat().st_size
    report["uasset_sha256"] = digest(asset_file)
    check("MODEL-SOURCE-UNCHANGED", digest(source) == report["source_sha256"])
    report["passed"] = True
except Exception:
    report["errors"].append(traceback.format_exc())
    unreal.log_error(report["errors"][-1])
finally:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("OVMODELIMPORT " + str(report["passed"]))
    unreal.SystemLibrary.quit_editor()
