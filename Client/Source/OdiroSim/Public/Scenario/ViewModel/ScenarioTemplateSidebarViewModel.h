#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioCoreTypes.h"
#include "Shared/ScenarioDocumentTypes.h"
#include "ScenarioTemplateSidebarViewModel.generated.h"

class UScenarioEditorUiSubsystem;
class UScenarioAuthoringSubsystem;
class UScenarioTemplateFieldRowViewModel;

// Scenario Template sidebar의 panel 선택 상태와 반복 field row 목록.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioTemplateSidebarViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// World subsystem 소유 관계를 연결하고 기본 field row를 구성한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem);

	// Active template panel을 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void SelectPanel(EScenarioTemplateSidebarPanel panel);

	// 기본 panel별 field row 목록을 다시 구성한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void RefreshDefaultFields();

	// 현재 active template panel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	EScenarioTemplateSidebarPanel GetActivePanel() const { return ActivePanel; }

	// 현재 active panel의 field row ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> GetVisibleFieldItems() const;

	// 현재 draft scenario를 가져온다.
	bool TryGetDraftScenario(FScenarioDocument& outScenario, FString& outFailureReason) const;

	// scenario_id metadata draft edit을 authoring subsystem에 위임한다.
	bool SetDraftScenarioId(const FString& scenarioId, TArray<FString>& outDiagnostics);

	// intent metadata draft edit을 authoring subsystem에 위임한다.
	bool SetDraftIntent(const FString& intent, TArray<FString>& outDiagnostics);

	// robot.start anchor draft edit을 authoring subsystem에 위임한다.
	bool SetDraftRobotStartAnchor(const FScenarioTemplateRobotAnchor& anchor, TArray<FString>& outDiagnostics);

	// robot.goal anchor draft edit을 authoring subsystem에 위임한다.
	bool SetDraftRobotGoalAnchor(const FScenarioTemplateRobotAnchor& anchor, TArray<FString>& outDiagnostics);

	// scenario_id text edit command를 처리한다.
	bool CommitScenarioIdText(const FText& text, FString& outStatusText);

	// intent text edit command를 처리한다.
	bool CommitIntentText(const FText& text, FString& outStatusText);

	// robot anchor text field edit command를 처리한다.
	bool CommitRobotAnchorText(
		EScenarioEditorSidebarRobotAnchorTarget target,
		EScenarioEditorSidebarRobotAnchorField field,
		const FText& text,
		FString& outStatusText);

	// robot anchor range field edit command를 처리한다.
	bool CommitRobotAnchorRange(
		EScenarioEditorSidebarRobotAnchorTarget target,
		EScenarioEditorSidebarRobotAnchorField field,
		const FText& minText,
		const FText& maxText,
		FString& outStatusText);

	// corridor surface catalog option을 authoring subsystem에서 읽는다.
	void GetCorridorSurfaceIdOptions(TArray<FString>& outSurfaceIds) const;

	// corridor walkway width draft edit을 authoring subsystem에 위임한다.
	bool SetCorridorWalkwayWidthMeters(
		const FScenarioTemplateNumberValue& widthMeters,
		TArray<FString>& outDiagnostics);

	// corridor side lane profile draft edit을 authoring subsystem에 위임한다.
	bool SetCorridorSideLaneProfile(
		EScenarioEditorCorridorSide side,
		const TArray<FScenarioTemplateLaneRule>& lanes,
		TArray<FString>& outDiagnostics);

	// corridor axis points draft edit을 authoring subsystem에 위임한다.
	bool SetCorridorAxisPointsMeters(const TArray<FVector2D>& pointsMeters, TArray<FString>& outDiagnostics);

	// corridor segment draft edit을 authoring subsystem에 위임한다.
	bool SetCorridorSegments(const TArray<FScenarioTemplateSegment>& segments, TArray<FString>& outDiagnostics);

	// corridor walkway_width_m fixed text edit command를 처리한다.
	bool CommitCorridorWalkwayWidthText(const FText& text, FString& outStatusText);

	// corridor walkway_width_m range text edit command를 처리한다.
	bool CommitCorridorWalkwayWidthRangeText(
		const FText& minText,
		const FText& maxText,
		FString& outStatusText);

	// corridor side lane surface edit command를 처리한다.
	bool CommitCorridorLaneSurfaceText(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FText& text,
		FString& outStatusText);

	// corridor side lane fixed width edit command를 처리한다.
	bool CommitCorridorLaneWidthText(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FText& text,
		FString& outStatusText);

	// corridor side lane width range edit command를 처리한다.
	bool CommitCorridorLaneWidthRangeText(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FText& minText,
		const FText& maxText,
		FString& outStatusText);

	// corridor side lane insertion command를 처리한다.
	bool AddCorridorLaneAfter(EScenarioEditorCorridorSide side, int32 laneIndex, FString& outStatusText);

	// corridor side lane removal command를 처리한다.
	bool RemoveCorridorLaneAt(EScenarioEditorCorridorSide side, int32 laneIndex, FString& outStatusText);

	// corridor axis point x edit command를 처리한다.
	bool CommitCorridorAxisPointXText(int32 pointIndex, const FText& text, FString& outStatusText);

	// corridor axis point y edit command를 처리한다.
	bool CommitCorridorAxisPointYText(int32 pointIndex, const FText& text, FString& outStatusText);

	// corridor axis point insertion command를 처리한다.
	bool AddCorridorAxisPointAfter(int32 pointIndex, FString& outStatusText);

	// corridor axis point removal command를 처리한다.
	bool RemoveCorridorAxisPointAt(int32 pointIndex, FString& outStatusText);

	// corridor segment id edit command를 처리한다.
	bool CommitCorridorSegmentIdText(int32 segmentIndex, const FText& text, FString& outStatusText);

	// corridor segment type edit command를 처리한다.
	bool CommitCorridorSegmentTypeText(int32 segmentIndex, const FText& text, FString& outStatusText);

	// corridor segment along_range_m edit command를 처리한다.
	bool CommitCorridorSegmentAlongRangeText(
		int32 segmentIndex,
		const FText& minText,
		const FText& maxText,
		FString& outStatusText);

	// corridor segment replaced_by edit command를 처리한다.
	bool CommitCorridorSegmentReplacedByText(int32 segmentIndex, const FText& text, FString& outStatusText);

	// corridor segment insertion command를 처리한다.
	bool AddCorridorSegmentAfter(int32 segmentIndex, FString& outStatusText);

	// corridor segment removal command를 처리한다.
	bool RemoveCorridorSegmentAt(int32 segmentIndex, FString& outStatusText);

	// obstacle prop catalog option을 authoring subsystem에서 읽는다.
	void GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const;

	// obstacle minimum clearance draft edit을 authoring subsystem에 위임한다.
	bool SetObstacleMinClearWidthMeters(
		const FScenarioTemplateNumberValue& widthMeters,
		TArray<FString>& outDiagnostics);

	// obstacle placement draft edit을 authoring subsystem에 위임한다.
	bool SetObstaclePlacements(
		const TArray<FScenarioTemplateObstaclePlacement>& placements,
		TArray<FString>& outDiagnostics);

	// obstacle min_clear_width_m fixed text edit command를 처리한다.
	bool CommitObstacleMinClearWidthText(const FText& text, FString& outStatusText);

	// obstacle min_clear_width_m range text edit command를 처리한다.
	bool CommitObstacleMinClearWidthRangeText(
		const FText& minText,
		const FText& maxText,
		FString& outStatusText);

	// obstacle placement text field edit command를 처리한다.
	bool CommitObstaclePlacementText(
		int32 placementIndex,
		EScenarioEditorSidebarObstaclePlacementField field,
		const FText& text,
		FString& outStatusText);

	// obstacle placement range field edit command를 처리한다.
	bool CommitObstaclePlacementRange(
		int32 placementIndex,
		EScenarioEditorSidebarObstaclePlacementField field,
		const FText& minText,
		const FText& maxText,
		FString& outStatusText);

	// obstacle placement insertion command를 처리한다.
	bool AddObstaclePlacementAfter(int32 placementIndex, FString& outStatusText);

	// obstacle placement removal command를 처리한다.
	bool RemoveObstaclePlacementAt(int32 placementIndex, FString& outStatusText);

	// 고정 number template 값을 생성한다.
	static FScenarioTemplateNumberValue MakeFixedTemplateNumberValue(double value);

	// 범위 number template 값을 생성한다.
	static FScenarioTemplateNumberValue MakeRangeTemplateNumberValue(double minValue, double maxValue);

	// 고정 integer template 값을 생성한다.
	static FScenarioTemplateIntegerValue MakeFixedTemplateIntegerValue(int32 value);

	// 범위 integer template 값을 생성한다.
	static FScenarioTemplateIntegerValue MakeRangeTemplateIntegerValue(int32 minValue, int32 maxValue);

	// Corridor polyline 길이를 meter 단위로 계산한다.
	static double MeasureAxisLengthMeters(const TArray<FVector2D>& pointsMeters);

	// Main panel field row ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> GetMainFieldItems() const;

	// Main draft template state를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void RefreshMainFieldItemsFromTemplate(const FScenarioDocument& scenarioTemplate);

	// Main field row item id로 ViewModel을 찾는다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	UScenarioTemplateFieldRowViewModel* FindMainFieldItem(const FString& fieldId) const;

	// Corridor panel field row ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> GetCorridorFieldItems() const;

	// Obstacle panel field row ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> GetObstacleFieldItems() const;

	// Pedestrian panel field row ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> GetPedestrianFieldItems() const;

	// Pedestrian draft template state를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void RefreshPedestrianFieldItemsFromTemplate(const FScenarioDocument& scenarioTemplate);

	// Pedestrian field row item id로 ViewModel을 찾는다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	UScenarioTemplateFieldRowViewModel* FindPedestrianFieldItem(const FString& fieldId) const;

	// Obstacle draft template state를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void RefreshObstacleFieldItemsFromTemplate(const FScenarioDocument& scenarioTemplate);

	// Obstacle field row item id로 ViewModel을 찾는다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	UScenarioTemplateFieldRowViewModel* FindObstacleFieldItem(const FString& fieldId) const;

	// Corridor draft template state를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	void RefreshCorridorFieldItemsFromTemplate(const FScenarioDocument& scenarioTemplate);

	// Corridor field row item id로 ViewModel을 찾는다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|TemplateSidebar")
	UScenarioTemplateFieldRowViewModel* FindCorridorFieldItem(const FString& fieldId) const;

	// Corridor axis point entry를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> CreateCorridorPointFieldItems(
		int32 pointIndex,
		const FVector2D& pointMeters);

	// Corridor side lane entry를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> CreateCorridorLaneFieldItems(
		EScenarioEditorCorridorSide side,
		int32 laneIndex,
		const FScenarioTemplateLaneRule& lane,
		const TArray<FString>& surfaceOptions);

	// Corridor segment entry를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> CreateCorridorSegmentFieldItems(
		int32 segmentIndex,
		const FScenarioTemplateSegment& segment,
		const TArray<FString>& surfaceOptions);

	// Obstacle placement entry를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> CreateObstaclePlacementFieldItems(
		int32 placementIndex,
		const FScenarioTemplateObstaclePlacement& placement);

	// Pedestrian encounter entry를 field row ViewModel 목록으로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|TemplateSidebar")
	TArray<UScenarioTemplateFieldRowViewModel*> CreatePedestrianEncounterFieldItems(
		int32 encounterIndex,
		const FScenarioTemplatePedestrianEncounter& encounter);

private:
	// UiSubsystem facade를 통해 authoring subsystem을 찾는다.
	UScenarioAuthoringSubsystem* ResolveAuthoringSubsystem() const;

	// Field row ViewModel을 생성한다.
	UScenarioTemplateFieldRowViewModel* CreateFieldItem(
		const FString& id,
		const FString& label,
		const FString& value,
		EScenarioEditorSidebarFieldInputType inputType,
		bool bEditable,
		bool bArrayControlsEnabled = false,
		bool bVisible = true);

	// Number template 값을 field row ViewModel 표시 상태로 변환한다.
	static void ApplyNumberFieldValue(
		UScenarioTemplateFieldRowViewModel* fieldItem,
		const FScenarioTemplateNumberValue& value);

	// Integer template 값을 field row ViewModel 표시 상태로 변환한다.
	static void ApplyIntegerFieldValue(
		UScenarioTemplateFieldRowViewModel* fieldItem,
		const FScenarioTemplateIntegerValue& value);

	// Field row item id로 ViewModel을 찾는다.
	static UScenarioTemplateFieldRowViewModel* FindFieldItemById(
		const TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>>& items,
		const FString& fieldId);

	// Robot anchor detail rows를 Main panel field row ViewModel 목록에 추가한다.
	void AppendRobotAnchorFieldItems(
		TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>>& items,
		const FString& idPrefix,
		const FScenarioTemplateRobotAnchor& anchor);

	// 현재 draft corridor segment id를 closed-set field option으로 캐시한다.
	void CacheCorridorSegmentOptions(const FScenarioDocument& scenarioTemplate);

	// Obstacle placement kind option 목록을 반환한다.
	static TArray<FString> GetObstaclePlacementKindOptions();

	// Obstacle pattern id option 목록을 반환한다.
	static TArray<FString> GetObstaclePatternOptions();

	// Corridor lane hint option 목록을 반환한다.
	static TArray<FString> GetLaneHintOptions();

	// Pedestrian encounter type option 목록을 반환한다.
	static TArray<FString> GetPedestrianEncounterTypeOptions();

	// Pedestrian persona option 목록을 반환한다.
	static TArray<FString> GetPedestrianPersonaOptions();

	// Boolean field option 목록을 반환한다.
	static TArray<FString> GetBooleanOptions();

	// Robot anchor type 표시 문자열을 반환한다.
	static FString RobotAnchorTypeToString(EScenarioTemplateRobotAnchorType type);

	// Robot heading 표시 문자열을 반환한다.
	static FString RobotHeadingToString(EScenarioTemplateRobotHeading heading);

	// Number template 값을 compact summary 표시 문자열로 변환한다.
	static FString FormatNumberSummary(const FScenarioTemplateNumberValue& value, const FString& suffix = FString());

	// Robot anchor summary 표시 문자열을 반환한다.
	static FString FormatRobotAnchorSummary(const FScenarioTemplateRobotAnchor& anchor);

	// 현재 draft corridor side lane profile을 command 처리용 값으로 복사한다.
	bool TryGetCorridorLaneProfile(
		EScenarioEditorCorridorSide side,
		TArray<FScenarioTemplateLaneRule>& outLanes,
		FString& outStatusText) const;

	// 현재 draft corridor axis point 목록을 command 처리용 값으로 복사한다.
	bool TryGetCorridorAxisPoints(TArray<FVector2D>& outPointsMeters, FString& outStatusText) const;

	// 현재 draft corridor segment 목록을 command 처리용 값으로 복사한다.
	bool TryGetCorridorSegments(TArray<FScenarioTemplateSegment>& outSegments, FString& outStatusText) const;

	// Validated corridor walkway width 값을 authoring subsystem에 반영한다.
	bool CommitCorridorWalkwayWidthValue(
		const FScenarioTemplateNumberValue& widthMeters,
		FString& outStatusText);

	// Corridor side lane profile 값을 authoring subsystem에 반영한다.
	bool CommitCorridorLaneProfile(
		EScenarioEditorCorridorSide side,
		const TArray<FScenarioTemplateLaneRule>& lanes,
		FString& outStatusText);

	// Corridor axis point 목록을 authoring subsystem에 반영한다.
	bool CommitCorridorAxisPoints(const TArray<FVector2D>& pointsMeters, FString& outStatusText);

	// Corridor segment 목록을 authoring subsystem에 반영한다.
	bool CommitCorridorSegments(const TArray<FScenarioTemplateSegment>& segments, FString& outStatusText);

	// Creates a valid default lane rule for one Corridor side.
	static FScenarioTemplateLaneRule MakeDefaultLaneRule(
		EScenarioEditorCorridorSide side,
		const TArray<FScenarioTemplateLaneRule>& existingLanes,
		int32 neighborIndex);

	// Creates a default axis point near another point.
	static FVector2D MakeDefaultAxisPoint(
		const TArray<FVector2D>& existingPoints,
		int32 neighborIndex);

	// Creates a valid default segment rule near another segment.
	FScenarioTemplateSegment MakeDefaultSegment(
		const TArray<FScenarioTemplateSegment>& existingSegments,
		int32 neighborIndex) const;

	// Parses one meter value from field row text.
	static bool TryParseMeters(const FText& text, double& outMeters);

	// Parses one corridor segment type from field row text.
	static bool TryParseSegmentType(const FText& text, EScenarioTemplateSegmentType& outType);

	// Diagnostics 목록을 sidebar status 문자열로 결합한다.
	static FString JoinDiagnostics(const TArray<FString>& diagnostics, const FString& fallbackText);

	// Commits one full robot anchor object through the authoring subsystem.
	bool CommitRobotAnchorValue(
		EScenarioEditorSidebarRobotAnchorTarget target,
		const FScenarioTemplateRobotAnchor& anchor,
		FString& outStatusText);

	// Parses one robot anchor type from editor text.
	static bool TryParseRobotAnchorType(const FText& text, EScenarioTemplateRobotAnchorType& outType);

	// Parses one robot heading hint from editor text.
	static bool TryParseRobotHeading(const FText& text, EScenarioTemplateRobotHeading& outHeading);

	// 현재 draft obstacle placement 목록을 command 처리용 값으로 복사한다.
	bool TryGetObstaclePlacements(
		TArray<FScenarioTemplateObstaclePlacement>& outPlacements,
		FString& outStatusText) const;

	// Validated obstacle min_clear_width_m 값을 authoring subsystem에 반영한다.
	bool CommitObstacleMinClearWidthValue(
		const FScenarioTemplateNumberValue& widthMeters,
		FString& outStatusText);

	// Obstacle placement 목록을 authoring subsystem에 반영한다.
	bool CommitObstaclePlacements(
		const TArray<FScenarioTemplateObstaclePlacement>& placements,
		FString& outStatusText);

	// Creates a default obstacle placement that can pass template validation.
	FScenarioTemplateObstaclePlacement MakeDefaultObstaclePlacement(
		const TArray<FScenarioTemplateObstaclePlacement>& existingPlacements,
		int32 neighborIndex) const;

	// Applies one number value to the selected placement field.
	static bool SetPlacementNumberField(
		FScenarioTemplateObstaclePlacement& placement,
		EScenarioEditorSidebarObstaclePlacementField field,
		const FScenarioTemplateNumberValue& value);

	// Applies one integer value to the selected placement field.
	static bool SetPlacementIntegerField(
		FScenarioTemplateObstaclePlacement& placement,
		EScenarioEditorSidebarObstaclePlacementField field,
		const FScenarioTemplateIntegerValue& value);

	// Parses one obstacle placement kind from editor text.
	static bool TryParsePlacementKind(const FText& text, EScenarioTemplateObstaclePlacementKind& outKind);

	// Parses one meter/degree scalar from field row text.
	static bool TryParseScalar(const FText& text, double& outValue);

	// Parses one optional number value from field row text.
	static bool TryParseOptionalNumber(const FText& text, FScenarioTemplateNumberValue& outValue);

	// Parses one optional number range from field row text.
	static bool TryParseOptionalNumberRange(
		const FText& minText,
		const FText& maxText,
		FScenarioTemplateNumberValue& outValue);

	// Parses one optional integer value from field row text.
	static bool TryParseOptionalInteger(const FText& text, FScenarioTemplateIntegerValue& outValue);

	// Parses one optional integer range from field row text.
	static bool TryParseOptionalIntegerRange(
		const FText& minText,
		const FText& maxText,
		FScenarioTemplateIntegerValue& outValue);

	// Parses one boolean value from field row text.
	static bool TryParseBool(const FText& text, bool& outValue);

	// Parses a comma-separated string list, trimming empty elements.
	static TArray<FString> ParseStringList(const FString& text);

	// Returns an unset optional number template value.
	static FScenarioTemplateNumberValue MakeUnsetNumberValue();

	// Returns an unset optional integer template value.
	static FScenarioTemplateIntegerValue MakeUnsetIntegerValue();

	// Pedestrian encounter type 표시 문자열을 반환한다.
	static FString EncounterTypeToString(EScenarioTemplateEncounterType type);

	// Obstacle placement kind 표시 문자열을 반환한다.
	static FString ObstaclePlacementKindToString(EScenarioTemplateObstaclePlacementKind kind);

	// Corridor axis type 표시 문자열을 반환한다.
	static FString AxisTypeToString(EScenarioCorridorAxisType type);

	// Corridor segment type 표시 문자열을 반환한다.
	static FString SegmentTypeToString(EScenarioTemplateSegmentType type);

	// Optional string template 값을 compact editor 표시 문자열로 변환한다.
	static FString FormatEditableStringValue(const FScenarioTemplateStringValue& value);

	// 문자열 목록을 field row 표시값으로 결합한다.
	static FString JoinStringList(const TArray<FString>& values);

	// Number editor 표시값을 포맷한다.
	static FString FormatEditableNumber(double value);

	// Integer editor 표시값을 포맷한다.
	static FString FormatEditableInteger(int32 value);

	// TObjectPtr 배열을 Blueprint-friendly raw pointer 배열로 복사한다.
	static TArray<UScenarioTemplateFieldRowViewModel*> CopyItems(
		const TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>>& source);

	// Subsystem command 접근 지점.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorUiSubsystem> UiSubsystem;

	// 현재 표시 중인 template sidebar panel.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	EScenarioTemplateSidebarPanel ActivePanel = EScenarioTemplateSidebarPanel::Main;

	// Main panel field row ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>> MainFieldItems;

	// Corridor panel field row ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>> CorridorFieldItems;

	// Obstacle panel field row ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>> ObstacleFieldItems;

	// Pedestrian panel field row ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|TemplateSidebar", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UScenarioTemplateFieldRowViewModel>> PedestrianFieldItems;

	// 현재 draft의 corridor segment id options.
	TArray<FString> CorridorSegmentIdOptions;
};
