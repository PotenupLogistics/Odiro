#pragma once

#include "CoreMinimal.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioSpecTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ScenarioEditorUiSubsystem.generated.h"

class AScenarioEditorController;
class UScenarioAssetPaletteViewModel;
class UScenarioAuthoringSubsystem;
class UScenarioEditorOutlinerViewModel;
class UScenarioEditorShellViewModel;
class UScenarioEditorToolbarViewModel;
class UScenarioLlmPromptViewModel;
class UScenarioPlaceableDetailsViewModel;
class UScenarioTemplateSidebarViewModel;
class UWidget;
struct FScenarioStaticObstaclePropEntry;

DECLARE_MULTICAST_DELEGATE_OneParam(FScenarioEditorUiAutoStartCompletedNative, bool /*bLoadedExistingScenario*/);

// ScenarioEditorMap world 단위 ViewModel lifecycle과 UI command orchestration을 소유한다.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorUiSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// World subsystem lifetime에 맞춰 Scenario Editor ViewModel 인스턴스를 생성한다.
	virtual void Initialize(FSubsystemCollectionBase& collection) override;

	// World teardown 시 ViewModel 참조를 정리한다.
	virtual void Deinitialize() override;

	// World context에서 ScenarioEditorMap UI subsystem을 반환한다.
	static UScenarioEditorUiSubsystem* ResolveForWorldContext(const UObject* worldContextObject);

	// Scenario editor auto-start 완료를 root view adapter에 중계한다.
	FScenarioEditorUiAutoStartCompletedNative& OnEditorAutoStartCompleted() { return EditorAutoStartCompletedEvent; }

	// Scenario editor launch subsystem의 auto-start 완료 여부를 반환한다.
	bool HasAutoStartedScenarioEditorSession() const;

	// Auto-start가 기존 scenario load로 끝났는지 반환한다.
	bool WasAutoStartedScenarioEditorSessionLoadedExistingScenario() const;

	// Root shell ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioEditorShellViewModel* GetShellViewModel() const { return ShellViewModel; }

	// Toolbar ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioEditorToolbarViewModel* GetToolbarViewModel() const { return ToolbarViewModel; }

	// Outliner ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioEditorOutlinerViewModel* GetOutlinerViewModel() const { return OutlinerViewModel; }

	// Asset palette ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioAssetPaletteViewModel* GetAssetPaletteViewModel() const { return AssetPaletteViewModel; }

	// Placeable details ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioPlaceableDetailsViewModel* GetPlaceableDetailsViewModel() const { return PlaceableDetailsViewModel; }

	// LLM prompt ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioLlmPromptViewModel* GetLlmPromptViewModel() const { return LlmPromptViewModel; }

	// Scenario template sidebar ViewModel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|UI")
	UScenarioTemplateSidebarViewModel* GetTemplateSidebarViewModel() const { return TemplateSidebarViewModel; }

	// Controller/authoring subsystem 상태를 ViewModel 표시 상태로 다시 동기화한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	void RefreshFromEditorState();

	// 현재 draft scenario를 저장하고 resolve된 파일 경로를 반환한다.
	bool SaveScenario(const FString& defaultSavePath, FString& outResolvedPath, TArray<FString>& outDiagnostics) const;

	// LLM authoring subsystem에 scenario generation 요청을 전달한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|UI")
	bool RequestScenarioGeneration(
		const FString& prompt,
		const FString& projectScenarioJsonPath,
		int32 episodeCount,
		FString& outFailureReason) const;

	// LLM generation 완료 delegate에 widget/adapter handler를 연결한다.
	bool BindScenarioGenerationCompleted(UObject* listener, FName functionName) const;

	// LLM generation 완료 delegate에서 widget/adapter handler를 제거한다.
	void UnbindScenarioGenerationCompleted(UObject* listener) const;

	// LLM authoring subsystem 기본 episode count를 반환한다.
	int32 GetDefaultScenarioGenerationEpisodeCount() const;

	// 최신 LLM generation 결과의 project scenario path를 반환한다.
	FString GetLatestGeneratedProjectScenarioPath() const;

	// 현재 editor source path를 <UserProject>/scenario.json으로 검증하고 project root를 반환한다.
	bool ResolveCurrentProjectScenarioPath(
		FString& outScenarioJsonPath,
		FString& outProjectPath,
		FString& outFailureReason) const;

	// 최신 generation 결과나 현재 editor source scenario를 editor draft로 다시 읽는다.
	bool LoadLatestGeneratedProjectScenario(FString& outStatusText) const;

	// 현재 editor draft를 저장하고 project run을 시작한다.
	bool RunCurrentProjectScenario(FString& outStatusText) const;

	// Startup map으로 돌아가는 map 전환 command를 수행한다.
	bool ReturnToStartup(const FString& startupMapId) const;

	// Details widget이 editor input mode를 요청한다.
	void RequestEditorWidgetInputMode(UWidget* requestingWidget) const;

	// Details widget이 요청했던 editor input mode를 해제한다.
	void ReleaseEditorWidgetInputMode(UWidget* requestingWidget) const;

	// 선택 transform gizmo 방향을 변경한다.
	bool SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode orientationMode) const;

	// 선택 transform gizmo 방향을 편집할 수 있는지 반환한다.
	bool CanEditTransformGizmoOrientationForSelection() const;

	// 현재 선택에 적용되는 transform gizmo 방향을 반환한다.
	EScenarioTransformGizmoOrientationMode GetEffectiveTransformGizmoOrientationMode() const;

	// 선택된 placeable을 삭제한다.
	bool DeleteSelectedPlaceable(FString& outFailureReason) const;

	// 선택된 placeable instance id를 변경한다.
	bool RenameSelectedPlaceableInstanceId(const FString& instanceId, FString& outFailureReason) const;

	// 선택된 placeable transform을 변경한다.
	bool UpdateSelectedPlaceableTransform(const FTransform& transform, FString& outFailureReason) const;

	// Static obstacle palette entry 목록을 controller에서 읽는다.
	void GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const;

	// Palette item placement command를 시작한다.
	bool BeginPalettePlacement(EScenarioPaletteItemType itemType, FName assetId) const;

	// Ground region draw command를 시작한다.
	bool BeginGroundRegionDraw(EScenarioGroundRegionType regionType) const;

	// 현재 world의 ScenarioEditorController를 반환한다.
	AScenarioEditorController* ResolveEditorController() const;

	// 현재 world의 ScenarioAuthoringSubsystem을 반환한다.
	UScenarioAuthoringSubsystem* ResolveAuthoringSubsystem() const;

private:
	// Default path와 loaded source path를 기준으로 실제 저장 경로를 계산한다.
	FString ResolveSavePath(const FString& defaultSavePath) const;
	// GameInstance 소유 ScenarioLlmAuthoringSubsystem을 반환한다.
	class UScenarioLlmAuthoringSubsystem* ResolveLlmAuthoringSubsystem() const;
	// GameInstance 소유 SimulatorLaunchSubsystem을 반환한다.
	class USimulatorLaunchSubsystem* ResolveSimulatorLaunchSubsystem() const;
	// GameInstance 소유 ScenarioEditorLaunchSubsystem을 반환한다.
	class UScenarioEditorLaunchSubsystem* ResolveScenarioEditorLaunchSubsystem() const;
	// Scenario editor launch auto-start 완료를 UI subsystem event로 중계한다.
	void HandleEditorAutoStartCompleted(bool bLoadedExistingScenario);
	// 현재 editor source scenario를 저장하고 project root를 반환한다.
	bool SaveCurrentProjectScenario(
		FString& outScenarioJsonPath,
		FString& outProjectPath,
		FString& outStatusText) const;

	// Root shell 표시 상태와 editor command facade.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorShellViewModel> ShellViewModel;

	// Toolbar 표시 상태와 저장/복귀 command facade.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorToolbarViewModel> ToolbarViewModel;

	// Outliner 반복 row 상태.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorOutlinerViewModel> OutlinerViewModel;

	// Asset palette 반복 tile 상태.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioAssetPaletteViewModel> AssetPaletteViewModel;

	// 선택된 placeable detail 표시 상태.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioPlaceableDetailsViewModel> PlaceableDetailsViewModel;

	// LLM prompt 표시/command 상태.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioLlmPromptViewModel> LlmPromptViewModel;

	// Scenario template sidebar 표시 상태.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioTemplateSidebarViewModel> TemplateSidebarViewModel;

	// Root widget adapter가 구독하는 auto-start 완료 relay.
	FScenarioEditorUiAutoStartCompletedNative EditorAutoStartCompletedEvent;

	// ScenarioEditorLaunchSubsystem auto-start delegate binding handle.
	FDelegateHandle EditorAutoStartCompletedHandle;
};
