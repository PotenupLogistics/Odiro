#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "ScenarioEditorToolbarViewModel.generated.h"

class UScenarioEditorUiSubsystem;
class UWidget;

// Scenario Editor toolbar의 표시 상태와 저장/복귀 command facade.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioEditorToolbarViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// World subsystem 소유 관계를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem);

	// WBP/adapter가 제공한 기본 저장 경로를 보관한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SetDefaultSavePath(const FString& path);

	// WBP/adapter가 제공한 startup map id를 보관한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SetStartupMapId(const FString& mapId);

	// 현재 draft scenario 저장 command를 Subsystem에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	bool SaveScenario();

	// Startup map 복귀 command를 Subsystem에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	bool ReturnToStartup();

	// Toolbar widget input mode를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void RequestEditorWidgetInputMode(UWidget* requestingWidget);

	// Toolbar widget input mode 요청을 해제한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void ReleaseEditorWidgetInputMode(UWidget* requestingWidget);

	// Toolbar의 active template sidebar panel을 선택한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SelectSidebarPanel(EScenarioTemplateSidebarPanel panel);

	// Toolbar 상태 표시 문자열을 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|Toolbar")
	void SetStatusText(const FString& message);

	// 현재 active template sidebar panel을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Toolbar")
	EScenarioTemplateSidebarPanel GetActiveSidebarPanel() const { return ActiveSidebarPanel; }

	// Toolbar 상태 표시 문자열을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Toolbar")
	FString GetStatusText() const { return StatusText; }

	// 저장 command 활성 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Toolbar")
	bool CanSave() const { return bCanSave; }

	// Startup 복귀 command 활성 상태를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|Toolbar")
	bool CanReturnToStartup() const { return bCanReturnToStartup; }

private:
	// Subsystem을 통해 save/open-level command에 접근한다.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorUiSubsystem> UiSubsystem;

	// 새 scenario 저장 시 사용할 기본 path template.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Toolbar", meta = (AllowPrivateAccess = "true"))
	FString DefaultSavePath = TEXT("Saved/UserProjects/ScenarioEditor/scenario.json");

	// Startup map으로 복귀할 때 사용할 map id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Toolbar", meta = (AllowPrivateAccess = "true"))
	FString StartupMapId = TEXT("StartupMap");

	// 현재 선택된 template sidebar panel.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Toolbar", meta = (AllowPrivateAccess = "true"))
	EScenarioTemplateSidebarPanel ActiveSidebarPanel = EScenarioTemplateSidebarPanel::Main;

	// Toolbar status line 표시 문자열.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Toolbar", meta = (AllowPrivateAccess = "true"))
	FString StatusText = TEXT("Ready");

	// Save button enabled 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Toolbar", meta = (AllowPrivateAccess = "true"))
	bool bCanSave = true;

	// Return button enabled 상태.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|Toolbar", meta = (AllowPrivateAccess = "true"))
	bool bCanReturnToStartup = true;
};
