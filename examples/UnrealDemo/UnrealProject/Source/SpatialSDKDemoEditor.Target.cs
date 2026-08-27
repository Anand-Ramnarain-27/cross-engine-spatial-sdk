using UnrealBuildTool;
using System.Collections.Generic;

public class SpatialSDKDemoEditorTarget : TargetRules
{
	public SpatialSDKDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("SpatialSDKDemo");
	}
}
