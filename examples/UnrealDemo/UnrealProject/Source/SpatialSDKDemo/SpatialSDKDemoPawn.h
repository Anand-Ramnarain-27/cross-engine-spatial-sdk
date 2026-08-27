#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SpatialSDKDemoPawn.generated.h"

class UCameraComponent;

// A camera positioned to see the generated DemoCity dataset (a 4x4 grid of
// 50m tiles centered on the origin) without needing a PlayerStart placed in
// a level — this project has no hand-authored level, see
// SpatialSDKDemoGameMode.h.
UCLASS()
class ASpatialSDKDemoPawn final : public APawn
{
	GENERATED_BODY()

public:
	ASpatialSDKDemoPawn();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Camera;
};
