using UnrealBuildTool;
public class OrbitalVanguardEditorTarget : TargetRules { public OrbitalVanguardEditorTarget(TargetInfo Target):base(Target) { Type=TargetType.Editor; DefaultBuildSettings=BuildSettingsVersion.V7; IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8; ExtraModuleNames.Add("OrbitalVanguard"); } }
