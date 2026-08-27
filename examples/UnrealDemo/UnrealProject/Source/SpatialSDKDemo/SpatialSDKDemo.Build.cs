using UnrealBuildTool;

public class SpatialSDKDemo : ModuleRules
{
	public SpatialSDKDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "SpatialSDKPlugin" });
	}
}
