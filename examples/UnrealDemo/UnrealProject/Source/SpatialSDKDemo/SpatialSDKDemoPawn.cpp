#include "SpatialSDKDemoPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"

ASpatialSDKDemoPawn::ASpatialSDKDemoPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->SetFieldOfView(60.0f);
}

void ASpatialSDKDemoPawn::BeginPlay()
{
	Super::BeginPlay();

	// DemoCity spans roughly [-10000, 10000]cm on X/Y and [0, 2000]cm up —
	// pull back along -X and up, pitched down, rather than relying on a
	// PlayerStart that doesn't exist in this project.
	SetActorLocationAndRotation(FVector(-16000.0f, 0.0f, 9000.0f), FRotator(-20.0f, 0.0f, 0.0f));
}
