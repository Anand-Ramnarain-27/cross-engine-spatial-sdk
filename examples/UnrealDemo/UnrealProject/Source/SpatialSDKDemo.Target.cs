using UnrealBuildTool;
using System.Collections.Generic;

public class SpatialSDKDemoTarget : TargetRules
{
	public SpatialSDKDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SpatialSDKDemo");
	}
}
