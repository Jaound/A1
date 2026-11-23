import unreal

task = unreal.AssetImportTask()
task.filename = r"C:\Windows\Fonts\malgun.ttf"
task.destination_path = "/Game/UI/Fonts"
task.destination_name = "Font_Malgun"
task.replace_existing = True
task.automated = True
task.save = True

task_bd = unreal.AssetImportTask()
task_bd.filename = r"C:\Windows\Fonts\malgunbd.ttf"
task_bd.destination_path = "/Game/UI/Fonts"
task_bd.destination_name = "Font_MalgunBold"
task_bd.replace_existing = True
task_bd.automated = True
task_bd.save = True

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.import_asset_tasks([task, task_bd])

print("Imported font 1:", task.get_editor_property("imported_object_paths"))
print("Imported font 2:", task_bd.get_editor_property("imported_object_paths"))

unreal.SystemLibrary.execute_console_command(None, "Quit")
