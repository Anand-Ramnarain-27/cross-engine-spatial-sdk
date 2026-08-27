#include "SpatialSDKDemoActor.h"

#include "Components/SceneComponent.h"
#include "SpatialWorldComponent.h"

ASpatialSDKDemoActor::ASpatialSDKDemoActor()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SpatialWorld = CreateDefaultSubobject<USpatialWorldComponent>(TEXT("SpatialWorld"));
}
