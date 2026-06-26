#pragma once

#include "CoreMinimal.h"
#include "UI/BaseCardWidget.h"
#include "BaseListItemWidget.generated.h"

class UImage;
class UTexture2D;
class UWidget;

// List item component with label, description, icon, and selection state.
UCLASS(BlueprintType, Blueprintable)
class ODIROSIM_API UBaseListItemWidget : public UBaseCardWidget
{
	GENERATED_BODY()

public:
	// Applies card state plus optional leading icon.
	virtual void SynchronizeBaseProperties() override;

	// Updates the optional leading icon texture.
	UFUNCTION(BlueprintCallable, Category = "UI|Base List Item")
	void SetIcon(UTexture2D* inIcon);

	// Returns the optional leading icon texture.
	UFUNCTION(BlueprintPure, Category = "UI|Base List Item")
	UTexture2D* GetIcon() const { return Icon; }

protected:
	// Optional leading icon texture.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "GetIcon", Setter = "SetIcon", BlueprintGetter = "GetIcon", BlueprintSetter = "SetIcon", Category = "UI|Base List Item", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTexture2D> Icon;

	// Icon visual owned by the Widget Blueprint.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UImage> IconImage;

	// Fixed-size wrapper that prevents optional icon textures from changing list row layout.
	UPROPERTY(Transient, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> IconBox;
};
