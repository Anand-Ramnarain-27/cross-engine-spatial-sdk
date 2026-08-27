#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialWorldComponent.h"
#include "SpatialSDKDemoActor.generated.h"

// The demo's one placed actor: a root SceneComponent plus a
// USpatialWorldComponent with its default settings (DemoCity dataset,
// debug visualization + stats overlay on). Spawned automatically by
// ASpatialSDKDemoGameMode so the demo runs with zero manual level setup —
// see SpatialSDKDemoGameMode.h for why that matters for this project.
UCLASS()
class ASpatialSDKDemoActor final : public AActor
{
	GENERATED_BODY()

public:
	ASpatialSDKDemoActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SpatialSDK")
	TObjectPtr<USpatialWorldComponent> SpatialWorld;
};
