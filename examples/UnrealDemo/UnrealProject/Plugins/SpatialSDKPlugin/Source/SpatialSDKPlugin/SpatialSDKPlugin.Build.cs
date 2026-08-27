using System.IO;
using UnrealBuildTool;

// Links SpatialUnrealPlugin.lib (a flat C ABI over spatial::SpatialWorld —
// see Source/ThirdParty/SpatialUnrealPlugin and
// examples/UnrealDemo/NativePlugin) as a prebuilt third-party binary rather
// than compiling the SDK's C++ sources as part of this module. That keeps
// UBT's toolchain/CRT settings for this module completely decoupled from
// whatever produced the DLL — see docs/unreal_integration.md for why that
// matters more here than it might look.
public class SpatialSDKPlugin : ModuleRules
{
	public SpatialSDKPlugin(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ProceduralMeshComponent",
		});

		string ThirdPartyDir = Path.Combine(PluginDirectory, "Source", "ThirdParty", "SpatialUnrealPlugin");
		PublicIncludePaths.Add(Path.Combine(ThirdPartyDir, "include"));

		string LibPath = Path.Combine(ThirdPartyDir, "lib", "SpatialUnrealPlugin.lib");
		PublicAdditionalLibraries.Add(LibPath);

		// Stages the runtime DLL next to this module's own binary so the
		// normal Windows DLL search path finds it — no delay-loading or
		// manual LoadLibrary needed.
		string DllName = "SpatialUnrealPlugin.dll";
		string DllPath = Path.Combine(ThirdPartyDir, "lib", DllName);
		RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", DllName), DllPath);
	}
}
