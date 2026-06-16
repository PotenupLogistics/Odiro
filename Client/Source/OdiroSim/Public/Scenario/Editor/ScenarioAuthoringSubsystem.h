#pragma once

#include "CoreMinimal.h"
#include "Scenario/Data/ScenarioCorridorSurfaceCatalog.h"
#include "Scenario/Data/ScenarioStaticObstaclePropCatalog.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioCompileTypes.h"
#include "Shared/ScenarioSpecTypes.h"
#include "Shared/ScenarioTemplateTypes.h"
#include "Scenario/Actors/ScenarioPedestrian.h"
#include "Scenario/Actors/ScenarioStaticObstacle.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioAuthoringSubsystem.generated.h"

class AScenarioStaticObstacle;
class AScenarioPedestrian;
class AScenarioGroundRegion;
class AScenarioCorridorHandleActor;
class AScenarioCorridorPreviewActor;
class AActor;
class FJsonObject;
class FJsonValue;
class UScenarioCorridorSurfaceCatalog;
class UScenarioCompiler;

UCLASS(BlueprintType)
class ODIROSIM_API UScenarioAuthoringSubsystem : public UWorldSubsystem
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

	// Catalog that resolves Corridor surface ids into editor-preview and semantic surface metadata.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario|Editor|Catalog")
	TSoftObjectPtr<UScenarioCorridorSurfaceCatalog> CorridorSurfaceCatalog;

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

	// 현재 draft에서 생성 preview actor를 다시 만들며 가능하면 Corridor authoring handle은 유지.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Preview")
	bool RefreshEditorPreviewFromDraft(TArray<FString>& outDiagnostics);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	void GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const;

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Palette")
	bool TryGetStaticObstaclePropEntry(FName propId, FScenarioStaticObstaclePropEntry& outPropEntry) const;

	// Returns Corridor surface entries from the configured catalog plus built-in fallback entries.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Corridor")
	void GetCorridorSurfaceEntries(TArray<FScenarioCorridorSurfaceEntry>& outEntries) const;

	// Resolves one Corridor surface id through the configured catalog and built-in fallback entries.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Corridor")
	bool TryGetCorridorSurfaceEntry(FName surfaceId, FScenarioCorridorSurfaceEntry& outSurfaceEntry) const;

	// Fixed numeric template value를 Blueprint 편집 UI에서 만들기 위한 helper임.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	static FScenarioTemplateNumberValue MakeFixedTemplateNumberValue(double value);

	// Range numeric template value를 Blueprint 편집 UI에서 만들기 위한 helper임.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Template")
	static FScenarioTemplateNumberValue MakeRangeTemplateNumberValue(double minValue, double maxValue);

	// 현재 draft의 corridor authoring source를 반환함.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Corridor")
	FScenarioTemplateCorridor GetDraftCorridor() const { return DraftScenarioTemplate.Corridor; }

	// Draft corridor axis polyline의 누적 길이를 meter 단위로 반환함.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Corridor")
	double GetDraftCorridorAxisLengthMeters() const;

	// Draft corridor axis polyline을 교체하고, along 기반 segment/reference 값을 새 길이에 맞게 보정함.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Corridor")
	bool SetCorridorAxisPointsMeters(const TArray<FVector2D>& pointsMeters, TArray<FString>& outDiagnostics);

	// Draft corridor walkway 폭을 fixed 또는 range 값으로 교체함.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Corridor")
	bool SetCorridorWalkwayWidthMeters(const FScenarioTemplateNumberValue& widthMeters, TArray<FString>& outDiagnostics);

	// Draft corridor의 한쪽 side lane profile 전체를 교체함.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Corridor")
	bool SetCorridorSideLaneProfile(
		EScenarioEditorCorridorSide side,
		const TArray<FScenarioTemplateLaneRule>& lanes,
		TArray<FString>& outDiagnostics);

	// Draft corridor segment 목록 전체를 교체함.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Corridor")
	bool SetCorridorSegments(
		const TArray<FScenarioTemplateSegment>& segments,
		TArray<FString>& outDiagnostics);

	// Corridor vertex handle transform result applied to the draft template axis.
	bool UpdateCorridorVertexHandleTransform(
		const FString& handleId,
		const FTransform& transform,
		FString& outFailureReason);

	// Corridor segment handle transform result applied to both vertices of a polyline edge.
	bool UpdateCorridorSegmentHandleTransform(
		const FString& handleId,
		const FTransform& transform,
		FString& outFailureReason);

	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	bool CanPlaceStaticObstacle(FName propId, const FTransform& transform, FString& outFailureReason) const;

	// Static obstacle 배치 transform을 현재 Corridor surface Z offset에 맞게 보정.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Placement")
	FTransform ResolveStaticObstaclePlacementTransform(const FTransform& transform) const;

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
	FScenarioWorldSpec GetDraftWorldSpec() const { return BuildDraftWorldSpecForPreview(); }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	FScenarioTemplateDocument GetDraftScenarioTemplate() const { return DraftScenarioTemplate; }

	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Authoring")
	FString GetSourceScenarioSetupJsonPath() const { return SourceScenarioTemplateJsonPath; }

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
	static void AppendSchemaDiagnostics(const TArray<FScenarioSchemaDiagnostic>& schemaDiagnostics, TArray<FString>& outDiagnostics);
	static FScenarioTemplateNumberValue MakeFixedTemplateNumber(double value);
	// Min/max 순서를 정규화한 range template number를 생성함.
	static FScenarioTemplateNumberValue MakeRangeTemplateNumber(double minValue, double maxValue);
	static FScenarioTemplateIntegerValue MakeFixedTemplateInteger(int32 value);
	static double GetFixedTemplateNumber(const FScenarioTemplateNumberValue& value, double defaultValue);
	// Template number가 width 같은 양수 필드에 쓸 수 있는지 확인함.
	static bool IsPositiveTemplateNumber(const FScenarioTemplateNumberValue& value);
	// Corridor axis polyline의 누적 길이를 meter 단위로 계산함.
	static double MeasureCorridorAxisLengthMeters(const TArray<FVector2D>& pointsMeters);
	// Corridor axis polyline 편집 입력의 기본 수치 조건을 검증함.
	static bool AreCorridorAxisPointsValid(const TArray<FVector2D>& pointsMeters, FString& outFailureReason);
	// Stable editor instance id for a corridor vertex handle.
	static FString MakeCorridorVertexHandleId(int32 vertexIndex);
	// Stable editor instance id for a corridor segment handle.
	static FString MakeCorridorSegmentHandleId(int32 segmentIndex);
	// Parses a corridor vertex handle id back into an axis point index.
	static bool TryParseCorridorVertexHandleId(const FString& handleId, int32& outVertexIndex);
	// Parses a corridor segment handle id back into a polyline edge index.
	static bool TryParseCorridorSegmentHandleId(const FString& handleId, int32& outSegmentIndex);
	// Converts an axis point in meters into the actor transform used by the vertex handle.
	static FTransform MakeCorridorVertexHandleTransform(const FVector2D& pointMeters);
	// Converts two axis points in meters into the actor transform used by the segment handle.
	static FTransform MakeCorridorSegmentHandleTransform(const FVector2D& startMeters, const FVector2D& endMeters);

	UScenarioCompiler* CreateScenarioCompiler() const;
	const UScenarioStaticObstaclePropCatalog* GetStaticObstaclePropCatalog() const;
	bool TryFindStaticObstacleProp(FName propId, FScenarioStaticObstaclePropEntry& outPropEntry) const;
	// Loads the configured Corridor surface catalog asset when available.
	const UScenarioCorridorSurfaceCatalog* GetCorridorSurfaceCatalog() const;
	// Resolves Corridor surface metadata through the configured catalog and built-in fallback entries.
	bool TryFindCorridorSurfaceEntry(FName surfaceId, FScenarioCorridorSurfaceEntry& outSurfaceEntry) const;
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
	bool IsDraftScenarioTemplateEmpty() const;
	void InitializeDraftDefaults();
	bool EnsureSingleRobotRouteSpec(TArray<FString>& outDiagnostics, bool& bOutDraftChanged);
	bool ValidateSingleRobotRouteSpecForExport(TArray<FString>& outDiagnostics) const;
	// Corridor 편집 결과를 schema 검증과 preview rebuild까지 통과한 경우에만 확정함.
	bool CommitCorridorDraftEdit(
		const FScenarioTemplateDocument& previousTemplate,
		bool bPreviousDirty,
		TArray<FString>& outDiagnostics);
	// Side lane profile 편집 입력이 surface와 width 조건을 만족하는지 확인함.
	bool ValidateCorridorLaneProfile(
		const TArray<FScenarioTemplateLaneRule>& lanes,
		const FString& path,
		TArray<FString>& outDiagnostics) const;
	// Segment 편집 입력이 axis 길이와 id 제약을 만족하는지 확인함.
	bool ValidateCorridorSegments(
		const TArray<FScenarioTemplateSegment>& segments,
		double axisLengthMeters,
		TArray<FString>& outDiagnostics) const;
	// Validates a single Corridor surface id against the configured catalog.
	bool ValidateCorridorSurfaceId(
		const FString& surfaceId,
		const FString& path,
		TArray<FString>& outDiagnostics) const;
	// Validates fixed or choice-based Corridor surface replacement values.
	bool ValidateCorridorSurfaceValue(
		const FScenarioTemplateStringValue& value,
		const FString& path,
		TArray<FString>& outDiagnostics) const;
	// Axis 길이 변경에 맞춰 along 기반 robot/obstacle reference 값을 같은 비율로 보정함.
	void RescaleCorridorAlongReferences(double oldLengthMeters, double newLengthMeters);
	// Axis 길이 변경에 맞춰 segment along range를 같은 비율로 보정함.
	void RescaleCorridorSegmentsForAxisLength(double oldLengthMeters, double newLengthMeters);
	// Along 값 기준으로 robot/obstacle reference의 segment id를 현재 segment 목록에 맞춤.
	void RepairCorridorReferenceSegmentIds();
	// Applies axis point edits without changing the scenario_template schema shape.
	bool ApplyCorridorAxisPointsEdit(
		const TArray<FVector2D>& pointsMeters,
		bool bRebuildAllPreviewActors,
		FString& outFailureReason);
	// Recreates generated preview actors while preserving Corridor handle actors.
	bool RefreshGeneratedEditorPreviewActorsFromDraft(TArray<FString>& outDiagnostics);
	// Destroys preview actors derived from the draft but leaves Corridor handles intact.
	void ClearGeneratedEditorPreviewActors();
	// Destroys Corridor handle actors owned by the authoring subsystem.
	void ClearCorridorHandleActors();
	// Creates the spline Corridor surface preview actor from the current draft axis and lane rules.
	bool SpawnCorridorPreviewActor(TArray<FString>& outDiagnostics);
	// Creates Corridor vertex and polyline segment handles from the current draft axis.
	bool SpawnCorridorHandleActors(TArray<FString>& outDiagnostics);
	// Updates existing Corridor handle transforms after the draft axis changes.
	void SyncCorridorHandleActors();
	// Along 위치를 포함하거나 가장 가까운 corridor segment id를 찾음.
	FString FindCorridorSegmentIdForAlongMeters(double alongMeters) const;
	// World 위치를 현재 corridor axis의 along/offset 좌표로 투영함.
	bool TryProjectLocationToCorridor(
		const FVector& locationCm,
		double& outAlongMeters,
		double& outOffsetMeters,
		FString& outSegmentId) const;
	// Corridor along/offset 좌표를 world XY 위치와 axis yaw로 해석함.
	bool TryResolveCorridorPoseMeters(
		double alongMeters,
		double offsetMeters,
		FVector2D& outPointMeters,
		double& outYawDegrees) const;
	// Corridor offset이 속한 surface band의 Z offset을 계산.
	double ResolveCorridorSurfaceZOffsetCm(double offsetMeters) const;
	// World 위치가 놓일 Corridor surface의 Z offset을 계산.
	bool TryResolveCorridorSurfaceZOffsetCm(const FVector& locationCm, double& outSurfaceZOffsetCm) const;
	// Draft template을 editor preview용 world spec으로 투영하고, 실패 시 compatibility projection으로 대체함.
	FScenarioWorldSpec BuildDraftWorldSpecForPreview(TArray<FString>* outDiagnostics = nullptr) const;
	// 기존 fixed obstacle/robot marker projection을 유지하는 fallback preview 경로임.
	FScenarioWorldSpec BuildCompatibilityDraftWorldSpecForPreview() const;
	// Editor preview projection이 사용하는 run config와 seed 값을 draft state로 맞춤.
	void ApplyEditorPreviewRunConfig(FScenarioWorldSpec& worldSpec) const;
	FScenarioTemplateRobotAnchor MakeRobotAnchorFromLocationCm(const FVector& locationCm) const;
	FVector ResolveRobotAnchorLocationCm(const FScenarioTemplateRobotAnchor& anchor, bool bGoalAnchor) const;
	FScenarioTemplateObstaclePlacement MakeStaticObstaclePlacement(
		const FString& placementId,
		FName propId,
		const FTransform& transform) const;
	FScenarioPlaceableInstanceSpec MakeStaticObstacleSpecFromPlacement(
		const FScenarioTemplateObstaclePlacement& placement) const;
	FScenarioPlaceableInstanceSpec MakeDeliveryBotSpecFromTemplateRobot() const;
	FScenarioTemplateObstaclePlacement* FindStaticObstaclePlacementByInstanceId(const FString& instanceId);
	const FScenarioTemplateObstaclePlacement* FindStaticObstaclePlacementByInstanceId(const FString& instanceId) const;
	void ImportWorldSpecAsScenarioTemplate(const FScenarioWorldSpec& worldSpec);
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
	FScenarioAuthoringStaticObstacleRecord* FindStaticObstacleRecordByInstanceId(const FString& instanceId);
	const FScenarioAuthoringStaticObstacleRecord* FindStaticObstacleRecordByInstanceId(const FString& instanceId) const;
	void ConfigureAuthoredStaticObstacleActor(
		AScenarioStaticObstacle* actor,
		const FScenarioPlaceableInstanceSpec& spec) const;

	UPROPERTY(Transient)
	FScenarioTemplateDocument DraftScenarioTemplate;

	UPROPERTY(Transient)
	TArray<FScenarioGroundRegionSpec> DraftGroundRegions;

	UPROPERTY(Transient)
	TArray<FScenarioDynamicActorSpec> DraftPedestrianSpecs;

	UPROPERTY(Transient)
	FString SourceScenarioTemplateJsonPath;

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

	// Editor-only spline preview for Corridor lane surfaces.
	UPROPERTY(Transient)
	TObjectPtr<AScenarioCorridorPreviewActor> CorridorPreviewActor;

	// Editor-only Corridor axis handles keyed by stable handle instance id.
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AScenarioCorridorHandleActor>> CorridorHandleActors;

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
