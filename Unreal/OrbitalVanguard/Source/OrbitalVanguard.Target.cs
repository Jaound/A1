using UnrealBuildTool;
public class OrbitalVanguardTarget : TargetRules { public OrbitalVanguardTarget(TargetInfo Target):base(Target) { Type=TargetType.Game; DefaultBuildSettings=BuildSettingsVersion.V7; IncludeOrderVersion=EngineIncludeOrderVersion.Unreal5_8; ExtraModuleNames.Add("OrbitalVanguard"); } }
