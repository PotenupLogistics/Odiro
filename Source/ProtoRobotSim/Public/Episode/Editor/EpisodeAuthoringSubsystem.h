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
	UEpisodeAuthoringSubsystem();

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<AActor> StartPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Classes")
	TSubclassOf<AActor> GoalPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Import")
	FString EpisodeSetupInputDirectory = TEXT("Json/Input");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "0.0"))
	double StaticObstacleGroundZToleranceCm = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Episode|Editor|Placement", meta = (ClampMin = "0.0"))
	double StaticObstacleFootprintClearanceCm = 5.0;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Authoring")
	void ClearDraft();

	// 새 에피소드 작성용 빈 draft 초기화.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Authoring")
	void NewDraft();

	// 기존 EpisodeSetup JSON을 UEpisodeCompiler로 컴파일하고, 성공하면 DraftWorldSpec으로 import.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Import")
	bool LoadEpisodeSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	// 문자열 기반 import.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Import")
	bool LoadEpisodeSetupJsonString(const FString& jsonString, TArray<FString>& outDiagnostics);

	// 이미 컴파일된 FEpisodeWorldSpec 직접 import.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Import")
	bool ImportCompiledWorldSpec(const FEpisodeWorldSpec& worldSpec, TArray<FString>& outDiagnostics);

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
	FEpisodeWorldSpec GetDraftWorldSpec() const { return DraftWorldSpec; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Authoring")
	FString GetSourceEpisodeSetupJsonPath() const { return SourceEpisodeSetupJsonPath; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Authoring")
	bool IsDraftDirty() const { return bDirty; }

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Authoring")
	TArray<FEpisodePlaceableInstanceSpec> GetAuthoredStaticObstacleSpecs() const;

	UFUNCTION(BlueprintPure, Category = "Episode|Editor|Authoring")
	TArray<FEpisodeAuthoringStaticObstacleRecord> GetAuthoredStaticObstacleRecords() const { return StaticObstacleRecords; }

	void GetAuthoredStaticObstacleActors(TArray<AEpisodeStaticObstacle*>& outActors) const;

	// DraftWorldSpec 전체 기준으로 JSON을 다시 작성하고, 다시 compiler로 round-trip 검증.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool ExportEpisodeSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool ExportAndValidateEpisodeSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	// 검증 성공 시 저장하고 dirty 상태 해제.
	UFUNCTION(BlueprintCallable, Category = "Episode|Editor|Export")
	bool SaveEpisodeSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

private:
	static constexpr double CentimetersToMeters = 0.01;

	static FString ResolveProjectRelativePath(const FString& filePath);
	FString ResolveEpisodeSetupLoadPath(const FString& filePath) const;
	static FString CompileSeverityToString(EEpisodeCompileDiagnosticSeverity severity);
	static void AppendCompileDiagnostics(const FEpisodeCompileResult& compileResult, TArray<FString>& outDiagnostics);
	static FString GroundRegionTypeToString(EEpisodeGroundRegionType regionType);
	static FString GroundShapeTypeToString(EEpisodeGroundShapeType shapeType);
	static TArray<TSharedPtr<FJsonValue>> MakeXyArrayMeters(const FVector& locationCm);
	static TArray<TSharedPtr<FJsonValue>> MakeSizeArrayMeters(const FVector2D& sizeCm);
	static TSharedPtr<FJsonObject> MakePropertiesObject(const TMap<FString, FEpisodeParamValue>& properties);
	static TSharedPtr<FJsonObject> MakeFilteredPropertiesObject(
		const TMap<FString, FEpisodeParamValue>& properties,
		const TSet<FString>& excludedKeys);
	static TSharedPtr<FJsonValue> MakeParamJsonValue(const FEpisodeParamValue& paramValue);
	static bool TryGetFloatProperty(const TMap<FString, FEpisodeParamValue>& properties, const FString& key, double& outValue);
	static bool TryGetBoolProperty(const TMap<FString, FEpisodeParamValue>& properties, const FString& key, bool& outValue);
	static bool TryGetStringProperty(const TMap<FString, FEpisodeParamValue>& properties, const FString& key, FString& outValue);

	bool TryFindStaticObstacleProp(FName propId, FEpisodeStaticObstaclePropEntry& outPropEntry) const;
	double ComputePlacementRadius2D(const FEpisodeStaticObstaclePropEntry& propEntry) const;
	FVector2D ComputePlacementHalfExtent2D(const FEpisodeStaticObstaclePropEntry& propEntry) const;
	bool StaticObstacleFootprintsOverlap(
		const FVector& candidateLocation,
		const FVector2D& candidateHalfExtent,
		const FEpisodeAuthoringStaticObstacleRecord& record) const;
	FString GenerateStaticObstacleInstanceId();
	bool ContainsInstanceId(const FString& instanceId) const;
	void InitializeDraftDefaults();
	void ClearEditorView();

	// import된 draft에서 정적 장애물만 AEpisodeStaticObstacle로 EditorMap에 재생성.
	bool RebuildEditorViewFromDraft(TArray<FString>& outDiagnostics);
	bool SpawnEditorStaticObstacleActor(
		const FEpisodePlaceableInstanceSpec& spec,
		AEpisodeStaticObstacle*& outActor,
		FString& outFailureReason);
	void SpawnRobotRouteMarkers(const FEpisodePlaceableInstanceSpec& spec, TArray<FString>& outDiagnostics);
	AActor* SpawnEditorMarkerActor(TSubclassOf<AActor> markerClass, const FTransform& transform);
	void AddStaticObstacleViewRecord(
		const FEpisodePlaceableInstanceSpec& spec,
		const FEpisodeStaticObstaclePropEntry& propEntry,
		AEpisodeStaticObstacle* actor);
	FEpisodePlaceableInstanceSpec MakeStaticObstacleSpec(
		const FString& instanceId,
		FName propId,
		const FTransform& transform) const;
	void ConfigureAuthoredStaticObstacleActor(
		AEpisodeStaticObstacle* actor,
		const FEpisodePlaceableInstanceSpec& spec) const;

	UPROPERTY(Transient)
	FEpisodeWorldSpec DraftWorldSpec;

	UPROPERTY(Transient)
	FString SourceEpisodeSetupJsonPath;

	UPROPERTY(Transient)
	bool bDirty = false;

	UPROPERTY(Transient)
	TArray<FEpisodeAuthoringStaticObstacleRecord> StaticObstacleRecords;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AEpisodeStaticObstacle>> StaticObstacleActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RouteMarkerActors;

	UPROPERTY(Transient)
	int32 NextStaticObstacleIndex = 1;
};
