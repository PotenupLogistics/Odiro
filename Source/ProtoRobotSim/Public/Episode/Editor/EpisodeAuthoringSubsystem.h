#pragma once

#include "CoreMinimal.h"
#include "Episode/Editor/EpisodeEditorTypes.h"
#include "Shared/EpisodeCompileTypes.h"
#include "Shared/EpisodeSpecTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "EpisodeAuthoringSubsystem.generated.h"

class AEpisodeStaticObstacle;
class FJsonObject;
class FJsonValue;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API UEpisodeAuthoringSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Export")
	FString ScenarioId = TEXT("episode_editor_export");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Export")
	FString MapId = TEXT("EpisodeEditorMap");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Export")
	int64 BaseSeed = 42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Export")
	int32 IterationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Export", meta = (ClampMin = "0.0"))
	double TimeLimitSeconds = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<AEpisodeStaticObstacle> StaticObstacleClass;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Authoring")
	void ClearDraft();

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Palette")
	void GetStaticObstaclePaletteEntries(TArray<FEpisodeStaticObstaclePropEntry>& outEntries) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	bool CanPlaceStaticObstacle(FName propId, const FTransform& transform, FString& outFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Placement")
	bool AddStaticObstacle(FName propId, const FTransform& transform, FEpisodePlaceableInstanceSpec& outSpec);

	bool AddStaticObstacleInternal(
		FName propId,
		const FTransform& transform,
		FEpisodePlaceableInstanceSpec& outSpec,
		AEpisodeStaticObstacle*& outActor);

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Authoring")
	TArray<FEpisodePlaceableInstanceSpec> GetAuthoredStaticObstacleSpecs() const { return StaticObstacleSpecs; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Authoring")
	TArray<FEpisodeAuthoringStaticObstacleRecord> GetAuthoredStaticObstacleRecords() const { return StaticObstacleRecords; }

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool ExportEpisodeSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool ExportAndValidateEpisodeSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool SaveEpisodeSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics) const;

private:
	static constexpr double CentimetersToMeters = 0.01;

	static FString ResolveProjectRelativePath(const FString& filePath);
	static FString CompileSeverityToString(EEpisodeCompileDiagnosticSeverity severity);
	static TArray<TSharedPtr<FJsonValue>> MakeXyArrayMeters(const FVector& locationCm);
	static TSharedPtr<FJsonObject> MakePropertiesObject(const TMap<FString, FEpisodeParamValue>& properties);
	static TSharedPtr<FJsonValue> MakeParamJsonValue(const FEpisodeParamValue& paramValue);

	bool TryFindStaticObstacleProp(FName propId, FEpisodeStaticObstaclePropEntry& outPropEntry) const;
	double ComputePlacementRadius2D(const FEpisodeStaticObstaclePropEntry& propEntry) const;
	FString GenerateStaticObstacleInstanceId();
	FEpisodePlaceableInstanceSpec MakeStaticObstacleSpec(
		const FString& instanceId,
		FName propId,
		const FTransform& transform) const;
	void ConfigureAuthoredStaticObstacleActor(
		AEpisodeStaticObstacle* actor,
		const FEpisodePlaceableInstanceSpec& spec) const;

	UPROPERTY(Transient)
	TArray<FEpisodePlaceableInstanceSpec> StaticObstacleSpecs;

	UPROPERTY(Transient)
	TArray<FEpisodeAuthoringStaticObstacleRecord> StaticObstacleRecords;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeStaticObstacle>> StaticObstacleActors;

	UPROPERTY(Transient)
	int32 NextStaticObstacleIndex = 1;
};
