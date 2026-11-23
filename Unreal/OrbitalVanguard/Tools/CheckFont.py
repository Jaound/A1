import unreal

print(">>> UNREAL PYTHON CHECK <<<")
print("Has FontFactory:", hasattr(unreal, "FontFactory"))
print("Has TrueTypeMultiFontFace:", hasattr(unreal, "TrueTypeMultiFontFace"))
print("Has Font:", hasattr(unreal, "Font"))

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
print("AssetTools:", asset_tools)
unreal.SystemLibrary.execute_console_command(None, "Quit")
