using UnrealBuildTool;
public class OrbitalVanguard : ModuleRules { public OrbitalVanguard(ReadOnlyTargetRules Target):base(Target) { PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs; PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","InputCore","UMG","Slate","SlateCore"}); } }
