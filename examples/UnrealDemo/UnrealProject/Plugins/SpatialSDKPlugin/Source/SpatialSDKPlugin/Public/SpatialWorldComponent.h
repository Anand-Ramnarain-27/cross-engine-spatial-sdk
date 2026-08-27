#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SpatialWorldComponent.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// The one component an integration adds to an Actor — the Unreal
// equivalent of examples/UnityDemo's SpatialWorldComponent.cs. No core SDK
// logic lives here: streaming, LOD selection, and the tile resource state
// machine all run in spatial::SpatialWorld inside SpatialUnrealPlugin.dll
// (see SpatialUnrealPlugin.h). This class marshals a camera in every tick,
// pulls draw commands out (already converted to Unreal-space — see
// UnrealCoordinateConversion.h), and keeps one UProceduralMeshComponent per
// resident tile-mesh up to date. See docs/unreal_integration.md.
UCLASS(ClassGroup = (SpatialSDK), meta = (BlueprintSpawnableComponent))
class SPATIALSDKPLUGIN_API USpatialWorldComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	USpatialWorldComponent();

	// Path to the .world manifest, relative to <ProjectDir>/SpatialSDKData/.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dataset")
	FString DatasetPath = TEXT("DemoCity/DemoCity.world");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "0"))
	float StreamingRadiusCm = 40000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1"))
	int32 MaxResidentTiles = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1"))
	int32 CpuMemoryBudgetMB = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1"))
	int32 WorkerThreadCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Streaming", meta = (ClampMin = "1"))
	int32 MaxGPUUploadsPerUpdate = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bEnableDebugVisualization = true;

	// Drives an on-screen GEngine debug-message overlay, the Unreal
	// equivalent of the Unity plugin's OnGUI stats block.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	bool bEnableStatistics = true;

	// Shared material tiles are drawn with; per-tile base color comes from
	// the tile's SDK material via a UMaterialInstanceDynamic created per
	// mesh component (the Unreal equivalent of Unity's MaterialPropertyBlock).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	TObjectPtr<UMaterialInterface> TileMaterial;

	// Drives streaming and LOD selection. Defaults to the first local
	// player's camera manager if left unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	TObjectPtr<AActor> CameraOverride;

	// "Maximum LOD" from the brief's suggested property list — dataset-
	// derived (how many LOD levels SpatialTileBuilder generated for the
	// loaded dataset), not a runtime cap the SDK has a concept of. Read-only
	// for the same reason examples/UnityDemo's DatasetMaxLOD is: a field
	// that looked editable but didn't do anything would be worse than not
	// having it.
	UPROPERTY(BlueprintReadOnly, Category = "Dataset")
	int32 DatasetMaxLOD = 0;

	UFUNCTION(BlueprintPure, Category = "SpatialSDK")
	bool IsDatasetLoaded() const { return bIsLoaded; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void* NativeWorld = nullptr;
	bool bIsLoaded = false;
	float TimeSinceLastStatsLog = 0.0f;

	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<UProceduralMeshComponent>> MeshComponentsByMeshId;

	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<UMaterialInstanceDynamic>> MaterialInstancesByMaterialId;

	bool ResolveCamera(FVector& OutLocation, FVector& OutForward, float& OutVerticalFovRadians, float& OutViewportHeightPx) const;
	void DrawResidentTiles();
	void DrawDebugLines() const;
	void ShowStatisticsOverlay() const;
	UProceduralMeshComponent* GetOrCreateMeshComponent(int64 MeshId, int64 MaterialId);
	UMaterialInstanceDynamic* GetOrCreateMaterialInstance(int64 MaterialId);
};
