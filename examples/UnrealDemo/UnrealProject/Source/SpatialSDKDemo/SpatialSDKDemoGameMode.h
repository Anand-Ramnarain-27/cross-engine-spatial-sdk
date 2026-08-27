#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpatialSDKDemoGameMode.generated.h"

// This project deliberately has no hand-authored level — placing an actor
// with USpatialWorldComponent and a camera via the level editor would need
// the same GUI interaction the rest of this repo's phases avoid relying on
// when it isn't available (see docs/unreal_integration.md). Instead this
// GameMode spawns the demo actor and positions the camera in code, so
// pressing Play (or running -game) on ANY level — including the engine's
// empty default — runs the full demo with zero manual setup.
UCLASS()
class ASpatialSDKDemoGameMode final : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpatialSDKDemoGameMode();

protected:
	virtual void BeginPlay() override;
};
