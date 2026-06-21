#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "DeliveryBotPointCloudReviewActor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class USceneComponent;
class UStaticMesh;

// Selects whether xyz points are interpreted as world coordinates or actor-local coordinates.
UENUM(BlueprintType)
enum class EDeliveryBotPointCloudCoordinateTypes : uint8
{
	World,
	ActorLocal
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
};

UCLASS(Blueprintable)
class ODIROSIM_API ADeliveryBotPointCloudReviewActor : public AActor
{
	GENERATED_BODY()

public:
	ADeliveryBotPointCloudReviewActor();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& transform) override;

public:
	// Loads XyzFilePath and rebuilds the point instances.
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DeliveryBot|PointCloud")
	bool LoadPointCloudFile();

	// 지정한 xyz 파일을 검증하고 point instance로 로드한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	bool LoadPointCloudFromFile(const FString& xyzFilePath);

	// Clears rendered instances and the in-memory point list.
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

	// 로드한 Point Cloud의 월드 표시 상태를 변경한다.
	UFUNCTION(BlueprintCallable, Category = "DeliveryBot|PointCloud")
	void SetPointCloudVisible(bool bVisible);

	// 로드한 Point Cloud가 현재 월드에 표시되는지 반환한다.
	UFUNCTION(BlueprintPure, Category = "DeliveryBot|PointCloud")
	bool IsPointCloudVisible() const;

private:
	// Builds the current scenario capture directory.
	FString BuildScenarioCaptureDirectory() const;

	// Builds the map_accumulated.xyz path inside one run directory.
	FString BuildMapAccumulatedFilePath(const FString& runDirectory) const;

	// Finds the newest map_accumulated.xyz path under the current scenario directory.
	bool TryFindLatestMapAccumulatedFilePath(FString& outFilePath) const;

	// Converts one xyz line into point data.
	bool ParseXyzLine(const FString& line, FDeliveryBotPointCloudReviewPointInfo& outPoint) const;

	// Converts point data into an instanced mesh transform.
	FTransform MakePointInstanceTransform(const FDeliveryBotPointCloudReviewPointInfo& point) const;

	// Resolves point data to its world-space debug draw location.
	FVector ResolvePointWorldLocation(const FDeliveryBotPointCloudReviewPointInfo& point) const;

	// Rebuilds instanced mesh points and optional debug color overlay.
	void RebuildPointInstances();

	// Draws the color debug overlay for loaded points.
	void DrawDebugColorOverlay() const;

private:
	// Actor root scene component.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<USceneComponent> SceneRoot;

	// Instanced mesh component that renders xyz points.
	UPROPERTY(VisibleAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> PointInstances;

	// Static mesh used for each point instance.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud")
	TObjectPtr<UStaticMesh> PointMesh;

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

	// Render size for each point instance in centimeters.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud", meta = (ClampMin = "0.1"))
	float PointSizeCm{ 5.f };

	// Maximum point count loaded from one xyz file.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud", meta = (ClampMin = "1"))
	int32 MaxPointCount{ 200000 };

	// Whether to draw color debug points in addition to mesh instances.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|Debug")
	bool bDrawDebugColorOverlay{ false };

	// Lifetime for debug color overlay points.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|Debug", meta = (ClampMin = "0.1"))
	float DebugOverlayLifeTimeSeconds{ 600.f };

	// Screen size for debug color overlay points.
	UPROPERTY(EditAnywhere, Category = "DeliveryBot|PointCloud|Debug", meta = (ClampMin = "1.0"))
	float DebugOverlayPointSize{ 5.f };

	// Last points parsed from the xyz file.
	TArray<FDeliveryBotPointCloudReviewPointInfo> LoadedPoints;
};
