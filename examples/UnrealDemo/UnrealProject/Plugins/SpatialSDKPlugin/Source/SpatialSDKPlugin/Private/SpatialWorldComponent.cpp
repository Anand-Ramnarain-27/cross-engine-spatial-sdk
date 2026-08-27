#include "SpatialWorldComponent.h"

#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Misc/Paths.h"

#include "SpatialUnrealPlugin.h"

namespace
{
	// SpatialUnreal_GetDrawCommands hands back a row-major matrix in the
	// SDK's column-vector convention (v' = M*v, translation in column 3 —
	// see spatial::core::Mat4). FMatrix is the opposite: row-vector
	// convention (v' = v*M, translation in ROW 3 — confirmed against
	// Engine/Source/.../Math/TranslationMatrix.h, not assumed), so building
	// FMatrix.M[row][col] straight from the SDK's m[row][col] would be
	// silently transposed. Currently invisible either way (the transform is
	// always identity — SpatialWorld never passes anything else — and
	// identity is its own transpose), but real once that's no longer true.
	FTransform BuildTransform(const TArray<float>& Transforms16, int32 CommandIndex)
	{
		const int32 Offset = CommandIndex * 16;
		FMatrix Matrix;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				Matrix.M[Row][Col] = Transforms16[Offset + (Col * 4) + Row];
			}
		}
		return FTransform(Matrix);
	}
}

USpatialWorldComponent::USpatialWorldComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USpatialWorldComponent::BeginPlay()
{
	Super::BeginPlay();

	NativeWorld = SpatialUnreal_CreateWorld();

	// Raw dataset files aren't cooked UAssets, so they live outside
	// Content/ under the project root — the same role StreamingAssets
	// plays for the Unity plugin.
	const FString FullPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("SpatialSDKData"), DatasetPath);

	SpatialUnrealLoadConfig Config{};
	Config.streamingRadiusCm = StreamingRadiusCm;
	Config.maxResidentTiles = static_cast<uint32>(FMath::Max(1, MaxResidentTiles));
	Config.cpuMemoryBudgetBytes = static_cast<uint64>(FMath::Max(1, CpuMemoryBudgetMB)) * 1024ull * 1024ull;
	Config.workerThreadCount = static_cast<uint32>(FMath::Max(1, WorkerThreadCount));
	Config.maxGPUUploadsPerUpdate = static_cast<uint32>(FMath::Max(1, MaxGPUUploadsPerUpdate));
	Config.debugVisualizationEnabled = bEnableDebugVisualization ? 1 : 0;

	const SpatialUnrealResult Result = SpatialUnreal_LoadDataset(NativeWorld, TCHAR_TO_UTF8(*FullPath), Config);
	bIsLoaded = (Result == SpatialUnrealResult_Ok);

	if (!bIsLoaded)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialWorldComponent: failed to load dataset '%s' (result=%d)"), *FullPath, static_cast<int32>(Result));
		return;
	}

	DatasetMaxLOD = static_cast<int32>(SpatialUnreal_GetDatasetMaxLOD(NativeWorld));
	UE_LOG(LogTemp, Log, TEXT("USpatialWorldComponent: loaded '%s', dataset max LOD %d"), *FullPath, DatasetMaxLOD);
}

void USpatialWorldComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (NativeWorld != nullptr)
	{
		// Shutdown() releases every GPU-side record ManagedMeshRenderer
		// tracks before DestroyWorld() frees the PluginState (renderer +
		// world) that owns them — same lifetime rule as SpatialUnityPlugin
		// and docs/sdk_api.md.
		SpatialUnreal_Shutdown(NativeWorld);
		SpatialUnreal_DestroyWorld(NativeWorld);
		NativeWorld = nullptr;
	}

	MeshComponentsByMeshId.Reset();
	MaterialInstancesByMaterialId.Reset();

	Super::EndPlay(EndPlayReason);
}

void USpatialWorldComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsLoaded)
	{
		return;
	}

	FVector Location, Forward;
	float VerticalFovRadians, ViewportHeightPx;
	if (!ResolveCamera(Location, Forward, VerticalFovRadians, ViewportHeightPx))
	{
		return;
	}

	SpatialUnrealCameraParams Camera{};
	Camera.posX = static_cast<float>(Location.X);
	Camera.posY = static_cast<float>(Location.Y);
	Camera.posZ = static_cast<float>(Location.Z);
	Camera.fwdX = static_cast<float>(Forward.X);
	Camera.fwdY = static_cast<float>(Forward.Y);
	Camera.fwdZ = static_cast<float>(Forward.Z);
	Camera.verticalFovRadians = VerticalFovRadians;
	Camera.viewportHeightPx = ViewportHeightPx;

	SpatialUnreal_SetDebugVisualization(NativeWorld, bEnableDebugVisualization ? 1 : 0);
	SpatialUnreal_Update(NativeWorld, Camera);
	SpatialUnreal_Render(NativeWorld, Camera);

	DrawResidentTiles();

	if (bEnableDebugVisualization)
	{
		DrawDebugLines();
	}

	if (bEnableStatistics)
	{
		ShowStatisticsOverlay();
	}

	// Mirrors the on-screen stats overlay to the log at a fixed cadence —
	// the only way to confirm streaming actually converged when run
	// somewhere the on-screen overlay can't be observed directly (e.g. an
	// automated -game smoke test with no attached display).
	TimeSinceLastStatsLog += DeltaTime;
	if (TimeSinceLastStatsLog >= 2.0f)
	{
		TimeSinceLastStatsLog = 0.0f;
		SpatialUnrealStatistics Stats{};
		SpatialUnreal_GetStatistics(NativeWorld, &Stats);
		UE_LOG(LogTemp, Log, TEXT("USpatialWorldComponent: resident=%llu loading=%llu requested=%llu drawCommands=%d"),
			Stats.residentCount, Stats.loadingCount, Stats.requestedCount, SpatialUnreal_GetDrawCommandCount(NativeWorld));
	}
}

bool USpatialWorldComponent::ResolveCamera(FVector& OutLocation, FVector& OutForward, float& OutVerticalFovRadians, float& OutViewportHeightPx) const
{
	if (IsValid(CameraOverride))
	{
		OutLocation = CameraOverride->GetActorLocation();
		OutForward = CameraOverride->GetActorForwardVector();
		OutVerticalFovRadians = FMath::DegreesToRadians(60.0f);
		OutViewportHeightPx = 1080.0f;
		return true;
	}

	const UWorld* World = GetWorld();
	APlayerController* PC = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (PC == nullptr || PC->PlayerCameraManager == nullptr)
	{
		return false;
	}

	OutLocation = PC->PlayerCameraManager->GetCameraLocation();
	OutForward = PC->PlayerCameraManager->GetCameraRotation().Vector();

	int32 ViewportWidth = 1920, ViewportHeight = 1080;
	PC->GetViewportSize(ViewportWidth, ViewportHeight);
	OutViewportHeightPx = static_cast<float>(FMath::Max(1, ViewportHeight));

	const float HorizontalFovRadians = FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle());
	const float AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(FMath::Max(1, ViewportHeight));
	OutVerticalFovRadians = 2.0f * FMath::Atan(FMath::Tan(HorizontalFovRadians * 0.5f) / FMath::Max(AspectRatio, KINDA_SMALL_NUMBER));
	return true;
}

void USpatialWorldComponent::DrawResidentTiles()
{
	const int32 DrawCount = SpatialUnreal_GetDrawCommandCount(NativeWorld);

	TArray<int64> MeshIds;
	TArray<int64> MaterialIds;
	TArray<float> Transforms;
	MeshIds.SetNumUninitialized(DrawCount);
	MaterialIds.SetNumUninitialized(DrawCount);
	Transforms.SetNumUninitialized(DrawCount * 16);
	if (DrawCount > 0)
	{
		SpatialUnreal_GetDrawCommands(NativeWorld, MeshIds.GetData(), MaterialIds.GetData(), Transforms.GetData());
	}

	TSet<int64> PresentThisFrame;
	PresentThisFrame.Reserve(DrawCount);

	for (int32 i = 0; i < DrawCount; ++i)
	{
		UProceduralMeshComponent* Comp = GetOrCreateMeshComponent(MeshIds[i], MaterialIds[i]);
		if (Comp == nullptr)
		{
			continue;
		}
		PresentThisFrame.Add(MeshIds[i]);
		Comp->SetVisibility(true);
		Comp->SetWorldTransform(BuildTransform(Transforms, i));
	}

	// Hide (don't destroy) components for meshes no longer drawn this frame
	// — geometry for a still-resident tile doesn't change, so recreating it
	// would be pure waste; a tile that leaves residency and later returns
	// reuses the same UProceduralMeshComponent.
	for (const auto& Pair : MeshComponentsByMeshId)
	{
		if (!PresentThisFrame.Contains(Pair.Key) && IsValid(Pair.Value))
		{
			Pair.Value->SetVisibility(false);
		}
	}
}

UProceduralMeshComponent* USpatialWorldComponent::GetOrCreateMeshComponent(int64 MeshId, int64 MaterialId)
{
	if (TObjectPtr<UProceduralMeshComponent>* Existing = MeshComponentsByMeshId.Find(MeshId))
	{
		return *Existing;
	}

	const int32 VertexCount = SpatialUnreal_GetMeshVertexCount(NativeWorld, MeshId);
	const int32 IndexCount = SpatialUnreal_GetMeshIndexCount(NativeWorld, MeshId);
	if (VertexCount == 0 || IndexCount == 0)
	{
		return nullptr;
	}

	TArray<float> RawPositions, RawNormals, RawUVs;
	TArray<int32> RawIndices;
	RawPositions.SetNumUninitialized(VertexCount * 3);
	RawNormals.SetNumUninitialized(VertexCount * 3);
	RawUVs.SetNumUninitialized(VertexCount * 2);
	RawIndices.SetNumUninitialized(IndexCount);
	if (SpatialUnreal_GetMeshData(NativeWorld, MeshId, RawPositions.GetData(), RawNormals.GetData(), RawUVs.GetData(), RawIndices.GetData()) == 0)
	{
		return nullptr;
	}

	TArray<FVector> Vertices;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	Vertices.SetNumUninitialized(VertexCount);
	Normals.SetNumUninitialized(VertexCount);
	UVs.SetNumUninitialized(VertexCount);
	for (int32 i = 0; i < VertexCount; ++i)
	{
		Vertices[i] = FVector(RawPositions[i * 3 + 0], RawPositions[i * 3 + 1], RawPositions[i * 3 + 2]);
		Normals[i] = FVector(RawNormals[i * 3 + 0], RawNormals[i * 3 + 1], RawNormals[i * 3 + 2]);
		UVs[i] = FVector2D(RawUVs[i * 2 + 0], RawUVs[i * 2 + 1]);
	}

	AActor* Owner = GetOwner();
	UProceduralMeshComponent* Comp = NewObject<UProceduralMeshComponent>(Owner, NAME_None, RF_Transient);
	Comp->RegisterComponent();
	if (Owner->GetRootComponent() != nullptr)
	{
		Comp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	Comp->CreateMeshSection(0, Vertices, RawIndices, Normals, UVs, TArray<FColor>(), TArray<FProcMeshTangent>(), /*bCreateCollision=*/false);

	if (UMaterialInstanceDynamic* MID = GetOrCreateMaterialInstance(MaterialId))
	{
		Comp->SetMaterial(0, MID);
	}

	MeshComponentsByMeshId.Add(MeshId, Comp);
	return Comp;
}

UMaterialInstanceDynamic* USpatialWorldComponent::GetOrCreateMaterialInstance(int64 MaterialId)
{
	if (TileMaterial == nullptr)
	{
		// No user-supplied material: leave the component's material unset
		// and let Unreal fall back to its own default engine material —
		// still solid and lit, just uncolored. See docs/unreal_integration.md.
		return nullptr;
	}

	if (TObjectPtr<UMaterialInstanceDynamic>* Existing = MaterialInstancesByMaterialId.Find(MaterialId))
	{
		return *Existing;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(TileMaterial, this);
	if (MID != nullptr && MaterialId != 0)
	{
		float Rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
		if (SpatialUnreal_GetMaterialColor(NativeWorld, MaterialId, Rgba) != 0)
		{
			// Harmless no-op if TileMaterial has no "BaseColor" vector
			// parameter — see the property tooltip for what to add.
			MID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(Rgba[0], Rgba[1], Rgba[2], Rgba[3]));
		}
	}

	MaterialInstancesByMaterialId.Add(MaterialId, MID);
	return MID;
}

void USpatialWorldComponent::DrawDebugLines() const
{
	const int32 VertexCount = SpatialUnreal_GetDebugLineVertexCount(NativeWorld);
	if (VertexCount == 0)
	{
		return;
	}

	TArray<float> Positions, Colors;
	Positions.SetNumUninitialized(VertexCount * 3);
	Colors.SetNumUninitialized(VertexCount * 4);
	SpatialUnreal_GetDebugLineData(NativeWorld, Positions.GetData(), Colors.GetData());

	UWorld* World = GetWorld();
	for (int32 i = 0; i + 1 < VertexCount; i += 2)
	{
		const FVector Start(Positions[i * 3 + 0], Positions[i * 3 + 1], Positions[i * 3 + 2]);
		const FVector End(Positions[(i + 1) * 3 + 0], Positions[(i + 1) * 3 + 1], Positions[(i + 1) * 3 + 2]);
		const FColor Color = FLinearColor(Colors[i * 4 + 0], Colors[i * 4 + 1], Colors[i * 4 + 2], Colors[i * 4 + 3]).ToFColor(false);
		// bPersistentLines=false, LifeTime=-1: draws for one frame, redrawn
		// every Tick — the standard "immediate" debug-line idiom in Unreal.
		DrawDebugLine(World, Start, End, Color, false, -1.0f, 0, 2.0f);
	}
}

void USpatialWorldComponent::ShowStatisticsOverlay() const
{
	if (GEngine == nullptr)
	{
		return;
	}

	SpatialUnrealStatistics Stats{};
	SpatialUnreal_GetStatistics(NativeWorld, &Stats);

	// Stable keys (base + offset) so each line updates in place instead of
	// stacking a new message every frame.
	constexpr int32 BaseKey = 0x53504457; // 'SPDW'
	GEngine->AddOnScreenDebugMessage(BaseKey + 0, 0.0f, FColor::White, FString::Printf(TEXT("Dataset max LOD: %d"), DatasetMaxLOD));
	GEngine->AddOnScreenDebugMessage(BaseKey + 1, 0.0f, FColor::White, FString::Printf(TEXT("Resident tiles: %llu"), Stats.residentCount));
	GEngine->AddOnScreenDebugMessage(BaseKey + 2, 0.0f, FColor::White, FString::Printf(TEXT("Loading tiles: %llu"), Stats.loadingCount));
	GEngine->AddOnScreenDebugMessage(BaseKey + 3, 0.0f, FColor::White, FString::Printf(TEXT("Requested tiles: %llu"), Stats.requestedCount));
	GEngine->AddOnScreenDebugMessage(BaseKey + 4, 0.0f, FColor::White, FString::Printf(TEXT("CPU memory: %.1f MB"), Stats.cpuMemoryUsedBytes / (1024.0 * 1024.0)));
	GEngine->AddOnScreenDebugMessage(BaseKey + 5, 0.0f, FColor::White, FString::Printf(TEXT("GPU memory: %.1f MB"), Stats.gpuMemoryUsedBytes / (1024.0 * 1024.0)));
}
