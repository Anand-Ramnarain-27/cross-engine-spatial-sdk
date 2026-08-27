#include "SpatialSDKDemoGameMode.h"

#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"

#include "SpatialSDKDemoActor.h"
#include "SpatialSDKDemoPawn.h"

ASpatialSDKDemoGameMode::ASpatialSDKDemoGameMode()
{
	DefaultPawnClass = ASpatialSDKDemoPawn::StaticClass();
}

void ASpatialSDKDemoGameMode::BeginPlay()
{
	Super::BeginPlay();

	GetWorld()->SpawnActor<ASpatialSDKDemoActor>(FVector::ZeroVector, FRotator::ZeroRotator);

	// Without this, the scene has no light at all: Unreal's default
	// material is lit (not unlit), so every UProceduralMeshComponent
	// section renders pure black — indistinguishable from the also-black
	// empty-sky background. Matches the Directional Light
	// examples/UnityDemo's DemoSceneBuilder spawns for the same reason.
	ADirectionalLight* Light = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-50.0f, -30.0f, 0.0f));
	if (Light != nullptr && Light->GetLightComponent() != nullptr)
	{
		Light->GetLightComponent()->Intensity = 3.0f;
		Light->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	}
}
