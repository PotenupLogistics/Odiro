#pragma once

#include "CoreMinimal.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioSpecTypes.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioAuthoringSubsystem.generated.h"

class AScenarioStaticObstacle;
class AScenarioPedestrian;
class AScenarioGroundRegion;
class AActor;
class FJsonObject;
class FJsonValue;
class UScenarioCompiler;

UCLASS(BlueprintType)
class PROTOROBOTSIM_API UScenarioAuthoringSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UScenarioAuthoringSubsystem();

	virtual void Deinitialize() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Export")
	FString ScenarioId = TEXT("episode_editor_export");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Export")
	FString MapId = TEXT("ScenarioEditorMap");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Export")
	int64 BaseSeed = 42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Export")
	int32 IterationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Export", meta = (ClampMin = "0.0"))
	double TimeLimitSeconds = 60.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AScenarioStaticObstacle> StaticObstacleClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AScenarioPedestrian> PedestrianClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AActor> PedestrianVisualizationActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AActor> StartPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AActor> GoalPointClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Classes")
	TSubclassOf<AScenarioGroundRegion> GroundRegionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Catalog")
	TSoftObjectPtr<UScenarioStaticObstaclePropCatalog> StaticObstaclePropCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Import")
	FString ScenarioSetupInputDirectory = TEXT("Json/Input");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Placement", meta = (ClampMin = "0.0"))
	double StaticObstacleGroundZToleranceCm = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Placement", meta = (ClampMin = "0.0"))
	double StaticObstacleFootprintClearanceCm = 5.0;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Authoring")
	void ClearDraft();

	// 새 에피소드 작성용 빈 draft 초기화.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Authoring")
	void NewDraft();

	// 기존 ScenarioSetup JSON을 UScenarioCompiler로 컴파일하고, 성공하면 DraftWorldSpec으로 import.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Import")
	bool LoadScenarioSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

	// 문자열 기반 import.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Import")
	bool LoadScenarioSetupJsonString(const FString& jsonString, TArray<FString>& outDiagnostics);

	// 이미 컴파일된 FScenarioWorldSpec 직접 import.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Import")
	bool ImportCompiledWorldSpec(const FScenarioWorldSpec& worldSpec, TArray<FString>& outDiagnostics);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	void GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	bool TryGetStaticObstaclePropEntry(FName propId, FScenarioStaticObstaclePropEntry& outPropEntry) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool CanPlaceStaticObstacle(FName propId, const FTransform& transform, FString& outFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool CanPlaceEditorGroundActor(const FTransform& transform, FString& outFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool CanUpdateStaticObstacleTransform(
		const FString& instanceId,
		const FTransform& transform,
		FString& outFailureReason) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool AddStaticObstacle(FName propId, const FTransform& transform, FScenarioPlaceableInstanceSpec& outSpec);

	bool AddPedestrian(
		FName archetypeId,
		const FTransform& transform,
		FScenarioDynamicActorSpec& outSpec,
		AActor*& outActor,
		FString& outFailureReason);

	bool SetRobotStartLocation(
		FName assetId,
		const FTransform& transform,
		FScenarioPlaceableInstanceSpec& outSpec,
		AActor*& outMarker,
		FString& outFailureReason);

	bool SetRobotGoalLocation(
		const FTransform& transform,
		FScenarioPlaceableInstanceSpec& outSpec,
		AActor*& outMarker,
		FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool UpdateStaticObstacleTransform(
		const FString& instanceId,
		const FTransform& transform,
		FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool UpdateRobotStartPointTransform(
		const FTransform& transform,
		FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool UpdateRobotGoalPointTransform(
		const FTransform& transform,
		FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool RenameStaticObstacleInstanceId(
		const FString& oldInstanceId,
		const FString& newInstanceId,
		FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool RemoveStaticObstacle(
		const FString& instanceId,
		FString& outFailureReason);

	// 사각형 지면 영역 하나를 draft에 추가하고 에디터 뷰 actor를 스폰함.
	bool AddGroundRegion(
		EScenarioGroundRegionType regionType,
		const FVector& centerCm,
		const FVector2D& sizeCm,
		double yawDegrees,
		FScenarioGroundRegionSpec& outSpec,
		FString& outFailureReason);

	// gizmo 편집 결과(이동·yaw 회전)를 region spec의 Center/YawDegrees에 반영함. Size는 불변.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool UpdateGroundRegionTransform(
		const FString& regionId,
		const FTransform& transform,
		FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool RemoveGroundRegion(
		const FString& regionId,
		FString& outFailureReason);

	bool AddStaticObstacleInternal(
		FName propId,
		const FTransform& transform,
		FScenarioPlaceableInstanceSpec& outSpec,
		AScenarioStaticObstacle*& outActor);

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	FScenarioWorldSpec GetDraftWorldSpec() const { return DraftWorldSpec; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	FString GetSourceScenarioSetupJsonPath() const { return SourceScenarioSetupJsonPath; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	bool IsDraftDirty() const { return bDirty; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	TArray<FScenarioPlaceableInstanceSpec> GetAuthoredStaticObstacleSpecs() const;

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	TArray<FScenarioAuthoringStaticObstacleRecord> GetAuthoredStaticObstacleRecords() const { return StaticObstacleRecords; }

	void GetAuthoredStaticObstacleActors(TArray<AScenarioStaticObstacle*>& outActors) const;

	void GetEditorPlacementIgnoredActors(TArray<AActor*>& outActors) const;

	// DraftWorldSpec 전체 기준으로 JSON을 다시 작성하고, 다시 compiler로 round-trip 검증.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Export")
	bool ExportScenarioSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Export")
	bool ExportAndValidateScenarioSetupJsonString(FString& outJsonString, TArray<FString>& outDiagnostics) const;

	// 검증 성공 시 저장하고 dirty 상태 해제.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Export")
	bool SaveScenarioSetupJsonFile(const FString& jsonFilePath, FString& outResolvedJsonFilePath, TArray<FString>& outDiagnostics);

private:
	static constexpr double CentimetersToMeters = 0.01;

	static FString ResolveProjectRelativePath(const FString& filePath);
	FString ResolveScenarioSetupLoadPath(const FString& filePath) const;
	static FString CompileSeverityToString(EScenarioCompileDiagnosticSeverity severity);
	static void AppendCompileDiagnostics(const FScenarioCompileResult& compileResult, TArray<FString>& outDiagnostics);
	static FString GroundRegionTypeToString(EScenarioGroundRegionType regionType);
	static FString GroundShapeTypeToString(EScenarioGroundShapeType shapeType);
	static TArray<TSharedPtr<FJsonValue>> MakeXyArrayMeters(const FVector& locationCm);
	static TArray<TSharedPtr<FJsonValue>> MakeSizeArrayMeters(const FVector2D& sizeCm);
	static TSharedPtr<FJsonObject> MakePropertiesObject(const TMap<FString, FScenarioParamValue>& properties);
	static TSharedPtr<FJsonObject> MakeFilteredPropertiesObject(
		const TMap<FString, FScenarioParamValue>& properties,
		const TSet<FString>& excludedKeys);
	static TSharedPtr<FJsonValue> MakeParamJsonValue(const FScenarioParamValue& paramValue);
	static bool TryGetFloatProperty(const TMap<FString, FScenarioParamValue>& properties, const FString& key, double& outValue);
	static bool TryGetBoolProperty(const TMap<FString, FScenarioParamValue>& properties, const FString& key, bool& outValue);
	static bool TryGetStringProperty(const TMap<FString, FScenarioParamValue>& properties, const FString& key, FString& outValue);

	UScenarioCompiler* CreateScenarioCompiler() const;
	const UScenarioStaticObstaclePropCatalog* GetStaticObstaclePropCatalog() const;
	bool TryFindStaticObstacleProp(FName propId, FScenarioStaticObstaclePropEntry& outPropEntry) const;
	bool CanPlaceStaticObstacleInternal(
		FName propId,
		const FTransform& transform,
		const FString& ignoredInstanceId,
		FString& outFailureReason) const;
	double ComputePlacementRadius2D(const FScenarioStaticObstaclePropEntry& propEntry) const;
	FVector2D ComputePlacementHalfExtent2D(const FScenarioStaticObstaclePropEntry& propEntry) const;
	bool StaticObstacleFootprintsOverlap(
		const FVector& candidateLocation,
		const FVector2D& candidateHalfExtent,
		const FScenarioAuthoringStaticObstacleRecord& record) const;
	FString GenerateStaticObstacleInstanceId();
	FString GeneratePedestrianInstanceId();
	FString GenerateGroundRegionId();
	bool ContainsInstanceId(const FString& instanceId) const;
	bool ContainsGroundRegionId(const FString& regionId) const;
	void InitializeDraftDefaults();
	bool EnsureSingleRobotRouteSpec(TArray<FString>& outDiagnostics, bool& bOutDraftChanged);
	bool ValidateSingleRobotRouteSpecForExport(TArray<FString>& outDiagnostics) const;
	void ClearEditorView();

	// import된 draft에서 정적 장애물만 AScenarioStaticObstacle로 EditorMap에 재생성.
	bool RebuildEditorViewFromDraft(TArray<FString>& outDiagnostics);
	bool SpawnEditorStaticObstacleActor(
		const FScenarioPlaceableInstanceSpec& spec,
		AScenarioStaticObstacle*& outActor,
		FString& outFailureReason);
	bool SpawnEditorPedestrianActor(
		const FScenarioDynamicActorSpec& spec,
		AActor*& outActor,
		FString& outFailureReason);
	bool SpawnEditorGroundRegionActor(
		const FScenarioGroundRegionSpec& spec,
		AScenarioGroundRegion*& outActor,
		FString& outFailureReason);
	FScenarioGroundRegionSpec MakeGroundRegionSpec(
		const FString& regionId,
		EScenarioGroundRegionType regionType,
		const FVector& centerCm,
		const FVector2D& sizeCm,
		double yawDegrees) const;
	bool SpawnRobotRouteMarkers(const FScenarioPlaceableInstanceSpec& spec, TArray<FString>& outDiagnostics);
	AActor* SpawnEditorMarkerActor(TSubclassOf<AActor> markerClass, const FTransform& transform);
	AActor* SpawnOrReplaceRouteMarker(
		TObjectPtr<AActor>& markerActor,
		TSubclassOf<AActor> markerClass,
		const FTransform& transform,
		EScenarioPlaceableAuthoringRole markerRole,
		FString& outFailureReason);
	bool ConfigureRobotRouteMarkerActor(
		AActor* markerActor,
		EScenarioPlaceableAuthoringRole markerRole,
		FString& outFailureReason) const;
	void AddStaticObstacleViewRecord(
		const FScenarioPlaceableInstanceSpec& spec,
		const FScenarioStaticObstaclePropEntry& propEntry,
		AScenarioStaticObstacle* actor);
	void AddPedestrianViewRecord(const FScenarioDynamicActorSpec& spec, AActor* actor);
	FScenarioPlaceableInstanceSpec MakeStaticObstacleSpec(
		const FString& instanceId,
		FName propId,
		const FTransform& transform) const;
	FScenarioDynamicActorSpec MakePedestrianSpec(
		const FString& instanceId,
		FName archetypeId,
		const FTransform& transform) const;
	FScenarioPlaceableInstanceSpec* FindDeliveryBotSpec();
	const FScenarioPlaceableInstanceSpec* FindDeliveryBotSpec() const;
	FScenarioPlaceableInstanceSpec* FindStaticObstacleSpecByInstanceId(const FString& instanceId);
	const FScenarioPlaceableInstanceSpec* FindStaticObstacleSpecByInstanceId(const FString& instanceId) const;
	FScenarioAuthoringStaticObstacleRecord* FindStaticObstacleRecordByInstanceId(const FString& instanceId);
	const FScenarioAuthoringStaticObstacleRecord* FindStaticObstacleRecordByInstanceId(const FString& instanceId) const;
	void ConfigureAuthoredStaticObstacleActor(
		AScenarioStaticObstacle* actor,
		const FScenarioPlaceableInstanceSpec& spec) const;

	UPROPERTY(Transient)
	FScenarioWorldSpec DraftWorldSpec;

	UPROPERTY(Transient)
	FString SourceScenarioSetupJsonPath;

	UPROPERTY(Transient)
	bool bDirty = false;

	UPROPERTY(Transient)
	TArray<FScenarioAuthoringStaticObstacleRecord> StaticObstacleRecords;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AScenarioStaticObstacle>> StaticObstacleActors;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AActor>> PedestrianActors;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AScenarioGroundRegion>> GroundRegionActors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> RouteMarkerActors;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RobotStartMarkerActor;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RobotGoalMarkerActor;

	UPROPERTY(Transient)
	int32 NextStaticObstacleIndex = 1;

	UPROPERTY(Transient)
	int32 NextPedestrianIndex = 1;

	UPROPERTY(Transient)
	int32 NextGroundRegionIndex = 1;
};
