#include "Modules/ModuleManager.h"

// Nothing to do at module load/unload — SpatialUnrealPlugin.dll is a normal
// linked import (see SpatialSDKPlugin.Build.cs), not manually loaded, and
// USpatialWorldComponent owns its own native SpatialWorld handle per
// instance rather than this module owning any shared state.
class FSpatialSDKPluginModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FSpatialSDKPluginModule, SpatialSDKPlugin)
