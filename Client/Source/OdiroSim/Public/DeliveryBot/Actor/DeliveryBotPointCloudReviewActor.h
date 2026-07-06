#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "DeliveryBotPointCloudReviewActor.generated.h"

class ULidarPointCloud;
class ULidarPointCloudComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;

// Selects whether xyz points are interpreted as world coordinates or actor-local coordinates.
UENUM(BlueprintType)
enum class EDeliveryBotPointCloudCoordinateTypes : uint8
{
	World,
	ActorLocal
};

// Selects the replay review plugin rendering profile used for the loaded point cloud.
UENUM(BlueprintType)
enum class EDeliveryBotPointCloudReviewRenderMode : uint8
{
	Plugin3D,
	TopDownProjection
};

// Stores one parsed xyz point and its optional RGB color.
struct FDeliveryBotPointCloudReviewPointInfo
{
	// Source xyz X coordinate.
	float X{ 0.f };

	// Source xyz Y coordinate.
	float Y{ 0.f };

	// Source xyz Z coordinate.
	float Z{ 0.f };

	// Source xyz RGB color, or white when the file omits color.
	FColor Color{ FColor::White };

	// Semantic classification inferred from the xyz RGB color.
	FName Classification{ TEXT("unknown") };
};

UCLASS(Blueprintable)
class ODIROSIM_API ADeliveryBotPointCloudReviewActor : public AActor
{
	GENERATED_BODY()

public:
	// Creates the point cloud review actor and its plugin rendering component.
	ADeliveryBotPointCloudReviewActor();

protected:
	// Optionally loads the configured point cloud when gameplay begins.
	virtual void BeginPlay() override;

	// Optionally reloads the configured point cloud after editor construction changes.
	virtual void OnConstruction(const FTransform& transform) override;

public:
	// Point cloud loading

	// Loads XyzFilePath and rebuilds the runtime plugin point cloud.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DeliveryBot|PointCloud")
	bool LoadPointCloudFile();

	// 지정한 xyz 파일을 검증하고 point instance로 로드한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	bool LoadPointCloudFromFile(const FString& xyzFilePath);

	// map_accumulated.xyz의 map-local 좌표를 source world 좌표로 복원해서 로드한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	bool LoadReplayMapPointCloudFromFile(
		const FString& xyzFilePath,
		const FVector& captureOriginCm,
		float importYAxisSign);

	// Clears the runtime plugin point cloud and the in-memory point list.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DeliveryBot|PointCloud")
	void ClearPointCloud();

	// Reloads the same XyzFilePath after clearing current points.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DeliveryBot|PointCloud")
	bool ReloadPointCloud();

	// Finds and loads the newest map_accumulated.xyz in the selected scenario folder.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	bool LoadLatestMapAccumulated();

	// Editor button wrapper for latest map_accumulated loading.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DeliveryBot|PointCloud", meta = (DisplayName = "Load Latest Map Accumulated"))
	void LoadLatestMapAccumulatedInEditor();

	// Returns the number of points loaded from the last xyz file.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|PointCloud")
	int32 GetLoadedPointCount() const { return LoadedPoints.Num(); }

public:
	// Point cloud visibility

	// 로드한 Point Cloud의 월드 표시 상태를 변경한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	void SetPointCloudVisible(bool bVisible);

	// 로드한 Point Cloud가 현재 월드에 표시되는지 반환한다.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|PointCloud")
	bool IsPointCloudVisible() const;

	// Sets the renderer used by replay review cameras.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	void SetReviewRenderMode(EDeliveryBotPointCloudReviewRenderMode NewMode);

	// Returns the renderer currently used by replay review cameras.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|PointCloud")
	EDeliveryBotPointCloudReviewRenderMode GetReviewRenderMode() const { return ReviewRenderMode; }

	// Applies runtime visual emphasis without changing the loaded point positions.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	void ConfigureReviewVisualStyle(
		float InPointSizeCm,
		float InTopDownSphereSizeCm,
		float InColorBrightnessMultiplier);

	// Offsets TopDown sphere fallback points from their source positions.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	void SetReviewTopDownSphereZOffset(float InZOffsetCm);

	// Enables the plugin point cloud renderer alongside the mesh fallback.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	void SetReviewPluginRendererEnabled(bool bEnabled);

private:
	// File path helpers

	// Resolves and validates one xyz file path.
	bool TryResolveXyzFilePath(const FString& xyzFilePath, FString& outResolvedFilePath) const;

	// Builds the current scenario capture directory.
	FString BuildScenarioCaptureDirectory() const;

	// Builds the map_accumulated.xyz path inside one run directory.
	FString BuildMapAccumulatedFilePath(const FString& runDirectory) const;

	// Finds the newest map_accumulated.xyz path under the current scenario directory.
	bool TryFindLatestMapAccumulatedFilePath(FString& outFilePath) const;

private:
	// Point conversion and rendering

	// Converts one xyz line into point data.
	bool ParseXyzLine(const FString& line, FDeliveryBotPointCloudReviewPointInfo& outPoint) const;

	// Resolves the semantic class encoded in one xyz RGB color.
	FName ResolvePointClassificationFromColor(const FColor& Color) const;

	// map_accumulated.xyz의 map-local 좌표를 source world 좌표로 변환한다.
	FVector ResolveReplayMapSourceWorldLocation(const FDeliveryBotPointCloudReviewPointInfo& point) const;

	// Resolves point data into the plugin point cloud component local space.
	FVector ResolvePointCloudLocalLocation(const FDeliveryBotPointCloudReviewPointInfo& point) const;

	// Resolves point data to its world-space debug draw location.
	FVector ResolvePointWorldLocation(const FDeliveryBotPointCloudReviewPointInfo& point) const;

	// Applies the actor's review brightness multiplier to one point color.
	FColor ResolveReviewDisplayColor(const FColor& Color) const;

	// Rebuilds the runtime Lidar Point Cloud asset and component.
	bool RebuildPointCloudAsset();

	// Applies replay review rendering options to the plugin point cloud component.
	void ConfigurePointCloudRendering();

	// Rebuilds the TopDown-only sphere instances from loaded point colors.
	bool BuildTopDownSphereInstances();

	// Clears the TopDown-only sphere instances.
	void ClearTopDownSphereInstances();

	// Applies common rendering settings to one TopDown sphere instance component.
	void ConfigureTopDownSphereInstanceComponent(UHierarchicalInstancedStaticMeshComponent* component) const;

	// Creates or updates TopDown sphere materials for semantic point colors.
	void ApplyTopDownSphereMaterials();

	// Returns the TopDown sphere component that matches one parsed point classification.
	UHierarchicalInstancedStaticMeshComponent* ResolveTopDownSphereComponentForClassification(const FName& classification) const;

	// Creates or updates one dynamic TopDown sphere material.
	UMaterialInstanceDynamic* GetOrCreateTopDownSphereMaterial(
		TObjectPtr<UMaterialInstanceDynamic>& materialSlot,
		const FColor& color);

	// Applies the active review render mode to owned render components.
	void ApplyReviewRenderMode();

	// Draws the color debug overlay for loaded points.
	void DrawDebugColorOverlay() const;

private:
	// Actor root scene component.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<USceneComponent> SceneRoot;

	// Plugin component that renders the loaded point cloud.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<ULidarPointCloudComponent> PointCloudComponent;

	// Instanced sphere fallback for ground-colored points.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TopDownGroundPointInstances;

	// Instanced sphere fallback for wall-colored points.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TopDownWallPointInstances;

	// Instanced sphere fallback for obstacle-colored points.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TopDownObstaclePointInstances;

	// Instanced sphere fallback for unknown-colored points.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TopDownUnknownPointInstances;

	// Transient runtime point cloud asset owned by this review actor.
	UPROPERTY(Transient)
	TObjectPtr<ULidarPointCloud> PointCloudAsset;

	// Static mesh used by TopDown-only sphere point instances.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|TopDown")
	TObjectPtr<UStaticMesh> TopDownSphereMesh;

	// Base material used to create TopDown semantic color materials.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|TopDown")
	TObjectPtr<UMaterialInterface> TopDownSphereBaseMaterial;

	// Runtime material for TopDown ground-colored point spheres.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TopDownGroundPointMaterial;

	// Runtime material for TopDown wall-colored point spheres.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TopDownWallPointMaterial;

	// Runtime material for TopDown obstacle-colored point spheres.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TopDownObstaclePointMaterial;

	// Runtime material for TopDown unknown-colored point spheres.
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TopDownUnknownPointMaterial;

	// Absolute or project-resolved xyz file path to load manually.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud")
	FFilePath XyzFilePath;

	// Scenario number used when finding the newest map_accumulated file.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud", meta = (ClampMin = "0"))
	int32 ScenarioNumber{ 1 };

	// Coordinate mode used to place loaded xyz points.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud")
	EDeliveryBotPointCloudCoordinateTypes CoordinateType{ EDeliveryBotPointCloudCoordinateTypes::World };

	// Whether BeginPlay should load XyzFilePath automatically.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud")
	bool bAutoLoadOnBeginPlay{ false };

	// Whether construction should load XyzFilePath automatically.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud")
	bool bAutoLoadOnConstruction{ false };

	// Plugin point sprite size used when rendering the cloud.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud", meta = (ClampMin = "0.1"))
	float PointSizeCm{ 24.0f };

	// Sphere diameter in centimeters used only by the TopDown renderer.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|TopDown", meta = (ClampMin = "0.1"))
	float TopDownSphereSizeCm{ 3.f };

	// Optional Z offset in centimeters used only by the TopDown sphere renderer.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|TopDown")
	float TopDownSphereZOffsetCm{ 0.0f };

	// Maximum point count loaded from one xyz file.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud", meta = (ClampMin = "1"))
	int32 MaxPointCount{ INT_MAX };

	// Whether to draw debug color points in addition to plugin rendering.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|Debug")
	bool bDrawDebugColorOverlay{ false };

	// Lifetime for debug color overlay points.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|Debug", meta = (ClampMin = "0.1"))
	float DebugOverlayLifeTimeSeconds{ 600.f };

	// Screen size for debug color overlay points.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|Debug", meta = (ClampMin = "1.0"))
	float DebugOverlayPointSize{ 5.f };

	// Runtime color multiplier used for emphasized replay point cloud layers.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|Style", meta = (ClampMin = "0.0"))
	float ColorBrightnessMultiplier{ 1.0f };

	// Whether the plugin point cloud renderer participates in review presentation.
	bool bReviewPluginRendererEnabled = true;

	// Capture origin used to restore map_accumulated.xyz points into source world coordinates.
	FVector MapCaptureOriginCm = FVector::ZeroVector;

	// Y-axis sign used to restore map_accumulated.xyz points into source world coordinates.
	float MapImportYAxisSign = -1.0f;

	// True when loaded xyz points should be interpreted as map-local replay points.
	bool bUseMapLocalImportTransform = false;

	// Active replay review render mode.
	UPROPERTY(Transient)
	EDeliveryBotPointCloudReviewRenderMode ReviewRenderMode = EDeliveryBotPointCloudReviewRenderMode::Plugin3D;

	// Last points parsed from the xyz file.
	TArray<FDeliveryBotPointCloudReviewPointInfo> LoadedPoints;
};
