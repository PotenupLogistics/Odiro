#pragma once

#include "CoreMinimal.h"
#include "Platform/ViewModel/OdiroViewModelBase.h"
#include "Scenario/Editor/ScenarioEditorTypes.h"
#include "Shared/ScenarioCoreTypes.h"
#include "Shared/ScenarioSpecTypes.h"
#include "ScenarioAssetPaletteViewModel.generated.h"

class UScenarioEditorListItemViewModel;
class UScenarioEditorUiSubsystem;
class UWidget;

// Scenario Editor asset palette의 tile 목록과 placement command 상태.
UCLASS(BlueprintType)
class ODIROSIM_API UScenarioAssetPaletteViewModel : public UOdiroViewModelBase
{
	GENERATED_BODY()

public:
	// World subsystem 소유 관계를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|AssetPalette")
	void InitializeForSubsystem(UScenarioEditorUiSubsystem* uiSubsystem);

	// Catalog entry 배열을 palette item ViewModel 배열로 변환한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|AssetPalette")
	void SetPaletteEntries(const TArray<FScenarioPaletteItemEntry>& entries);

	// 선택된 palette asset id를 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|AssetPalette")
	void SelectAsset(FName assetId);

	// 선택된 palette asset id를 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|AssetPalette")
	FName GetSelectedAssetId() const { return SelectedAssetId; }

	// Palette tile ViewModel 목록을 반환한다.
	UFUNCTION(BlueprintPure, Category = "Scenario|Editor|AssetPalette")
	TArray<UScenarioEditorListItemViewModel*> GetItems() const;

	// Palette widget input mode를 요청한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|AssetPalette")
	void RequestEditorWidgetInputMode(UWidget* requestingWidget);

	// Palette widget input mode 요청을 해제한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|AssetPalette")
	void ReleaseEditorWidgetInputMode(UWidget* requestingWidget);

	// Static obstacle palette entry 목록을 controller/subsystem에서 읽는다.
	void GetStaticObstaclePaletteEntries(TArray<FScenarioStaticObstaclePropEntry>& outEntries) const;

	// Palette placement command를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|AssetPalette")
	bool BeginPalettePlacement(EScenarioPaletteItemType itemType, FName assetId);

	// Ground region draw command를 시작한다.
	UFUNCTION(BlueprintCallable, Category = "Scenario|Editor|AssetPalette")
	bool BeginGroundRegionDraw(EScenarioGroundRegionType regionType);

private:
	// Subsystem command 접근 지점.
	UPROPERTY(Transient)
	TObjectPtr<UScenarioEditorUiSubsystem> UiSubsystem;

	// 현재 선택된 palette asset id.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|AssetPalette", meta = (AllowPrivateAccess = "true"))
	FName SelectedAssetId;

	// Palette tile ViewModel 목록.
	UPROPERTY(BlueprintReadOnly, FieldNotify, Category = "Scenario|Editor|AssetPalette", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<UScenarioEditorListItemViewModel>> Items;
};
