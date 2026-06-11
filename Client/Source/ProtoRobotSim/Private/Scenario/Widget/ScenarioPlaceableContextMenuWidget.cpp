#include "Scenario/Widget/ScenarioPlaceableContextMenuWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Scenario/Components/ScenarioPlaceableComponent.h"
#include "Scenario/Editor/ScenarioEditorController.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogScenarioPlaceableContextMenuWidget, Log, All);

namespace
{
	FText MakeNumberText(double value)
	{
		return FText::FromString(FString::Printf(TEXT("%.3f"), value));
	}

	FString TransformFieldName(EScenarioPlaceableContextMenuTransformField field)
	{
		switch (field)
		{
		case EScenarioPlaceableContextMenuTransformField::LocationX:
			return TEXT("LocationX");
		case EScenarioPlaceableContextMenuTransformField::LocationY:
			return TEXT("LocationY");
		case EScenarioPlaceableContextMenuTransformField::LocationZ:
			return TEXT("LocationZ");
		case EScenarioPlaceableContextMenuTransformField::RotationX:
			return TEXT("RotationX");
		case EScenarioPlaceableContextMenuTransformField::RotationY:
			return TEXT("RotationY");
		case EScenarioPlaceableContextMenuTransformField::RotationZ:
			return TEXT("RotationZ");
		case EScenarioPlaceableContextMenuTransformField::ScaleX:
			return TEXT("ScaleX");
		case EScenarioPlaceableContextMenuTransformField::ScaleY:
			return TEXT("ScaleY");
		case EScenarioPlaceableContextMenuTransformField::ScaleZ:
			return TEXT("ScaleZ");
		default:
			return TEXT("Unknown");
		}
	}
}

void UScenarioPlaceableContextMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EditInstanceIdButton)
	{
		EditInstanceIdButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleEditInstanceIdButtonClicked);
		EditInstanceIdButton->OnClicked.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleEditInstanceIdButtonClicked);
	}
	if (WorldOrientationButton)
	{
		WorldOrientationButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleWorldOrientationButtonClicked);
		WorldOrientationButton->OnClicked.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleWorldOrientationButtonClicked);
	}
	if (RelativeOrientationButton)
	{
		RelativeOrientationButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRelativeOrientationButtonClicked);
		RelativeOrientationButton->OnClicked.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRelativeOrientationButtonClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleDeleteButtonClicked);
		DeleteButton->OnClicked.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleDeleteButtonClicked);
	}
	if (InstanceIdEditableText)
	{
		InstanceIdEditableText->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleInstanceIdCommitted);
		InstanceIdEditableText->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleInstanceIdCommitted);
	}

	if (LocationXTextBox)
	{
		LocationXTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleLocationXCommitted);
		LocationXTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleLocationXCommitted);
	}
	if (LocationYTextBox)
	{
		LocationYTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleLocationYCommitted);
		LocationYTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleLocationYCommitted);
	}
	if (LocationZTextBox)
	{
		LocationZTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleLocationZCommitted);
		LocationZTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleLocationZCommitted);
	}

	if (RotationXTextBox)
	{
		RotationXTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRotationXCommitted);
		RotationXTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRotationXCommitted);
	}
	if (RotationYTextBox)
	{
		RotationYTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRotationYCommitted);
		RotationYTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRotationYCommitted);
	}
	if (RotationZTextBox)
	{
		RotationZTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRotationZCommitted);
		RotationZTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleRotationZCommitted);
	}

	if (ScaleXTextBox)
	{
		ScaleXTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleScaleXCommitted);
		ScaleXTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleScaleXCommitted);
	}
	if (ScaleYTextBox)
	{
		ScaleYTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleScaleYCommitted);
		ScaleYTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleScaleYCommitted);
	}
	if (ScaleZTextBox)
	{
		ScaleZTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleScaleZCommitted);
		ScaleZTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableContextMenuWidget::HandleScaleZCommitted);
	}
}

void UScenarioPlaceableContextMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RequestEditorWidgetInputMode();
	RefreshFromSelectedPlaceable();
}

void UScenarioPlaceableContextMenuWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

void UScenarioPlaceableContextMenuWidget::SetSelectedPlaceable(UScenarioPlaceableComponent* placeableComponent)
{
	SelectedPlaceableComponent = placeableComponent;
	RefreshFromSelectedPlaceable();
}

void UScenarioPlaceableContextMenuWidget::RefreshFromSelectedPlaceable()
{
	bRefreshingFields = true;

	UScenarioPlaceableComponent* placeableComponent = SelectedPlaceableComponent.Get();
	AActor* ownerActor = placeableComponent ? placeableComponent->GetOwner() : nullptr;
	if (!placeableComponent || !ownerActor)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		bRefreshingFields = false;
		return;
	}

	SetVisibility(ESlateVisibility::Visible);
	SetInstanceIdFieldText(placeableComponent->InstanceId);
	if (AssetNameTextBlock)
	{
		AssetNameTextBlock->SetText(FText::FromString(placeableComponent->AssetId));
	}

	SetTransformFieldTexts(ownerActor->GetActorTransform());
	ApplyEditPermissions(placeableComponent);
	ApplyOrientationControls(placeableComponent);
	bRefreshingFields = false;
}

void UScenarioPlaceableContextMenuWidget::HandleEditInstanceIdButtonClicked()
{
	if (!InstanceIdEditableText)
	{
		return;
	}

	InstanceIdEditableText->SetKeyboardFocus();
}

void UScenarioPlaceableContextMenuWidget::HandleWorldOrientationButtonClicked()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode::World);
		RefreshFromSelectedPlaceable();
	}
}

void UScenarioPlaceableContextMenuWidget::HandleRelativeOrientationButtonClicked()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode::Relative);
		RefreshFromSelectedPlaceable();
	}
}

void UScenarioPlaceableContextMenuWidget::HandleDeleteButtonClicked()
{
	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return;
	}

	FString failureReason;
	UScenarioPlaceableComponent* placeableComponent = SelectedPlaceableComponent.Get();
	if (!placeableComponent || !placeableComponent->bAuthoringDeletable)
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Selected placeable cannot be deleted."));
		FlashInvalidInstanceIdField();
		return;
	}

	if (!editorController->DeleteSelectedPlaceable(failureReason))
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Failed to delete selected placeable | %s"), *failureReason);
		FlashInvalidInstanceIdField();
	}
}

void UScenarioPlaceableContextMenuWidget::HandleInstanceIdCommitted(const FText& text, ETextCommit::Type commitMethod)
{
	if (bRefreshingFields || commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromSelectedPlaceable();
		return;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return;
	}

	UScenarioPlaceableComponent* placeableComponent = SelectedPlaceableComponent.Get();
	if (!placeableComponent || !placeableComponent->bAuthoringRenamable)
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Selected placeable cannot be renamed."));
		RefreshFromSelectedPlaceable();
		FlashInvalidInstanceIdField();
		return;
	}

	FString failureReason;
	if (!editorController->TryRenameSelectedPlaceableInstanceId(text.ToString().TrimStartAndEnd(), failureReason))
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Rejected InstanceId edit | %s"), *failureReason);
		RefreshFromSelectedPlaceable();
		FlashInvalidInstanceIdField();
		return;
	}

	RefreshFromSelectedPlaceable();
}

void UScenarioPlaceableContextMenuWidget::HandleLocationXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationXTextBox, EScenarioPlaceableContextMenuTransformField::LocationX, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleLocationYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationYTextBox, EScenarioPlaceableContextMenuTransformField::LocationY, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleLocationZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationZTextBox, EScenarioPlaceableContextMenuTransformField::LocationZ, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleRotationXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationXTextBox, EScenarioPlaceableContextMenuTransformField::RotationX, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleRotationYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationYTextBox, EScenarioPlaceableContextMenuTransformField::RotationY, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleRotationZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationZTextBox, EScenarioPlaceableContextMenuTransformField::RotationZ, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleScaleXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleXTextBox, EScenarioPlaceableContextMenuTransformField::ScaleX, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleScaleYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleYTextBox, EScenarioPlaceableContextMenuTransformField::ScaleY, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::HandleScaleZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleZTextBox, EScenarioPlaceableContextMenuTransformField::ScaleZ, commitMethod);
}

void UScenarioPlaceableContextMenuWidget::RequestEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->RequestEditorWidgetInputMode(this);
	}
}

void UScenarioPlaceableContextMenuWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->ReleaseEditorWidgetInputMode(this);
	}
}

bool UScenarioPlaceableContextMenuWidget::CommitTransformField(
	UEditableTextBox* textBox,
	EScenarioPlaceableContextMenuTransformField field,
	ETextCommit::Type commitMethod)
{
	if (bRefreshingFields)
	{
		return false;
	}
	if (commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromSelectedPlaceable();
		return false;
	}
	UScenarioPlaceableComponent* placeableComponent = SelectedPlaceableComponent.Get();
	if (!IsTransformFieldEditable(placeableComponent, field))
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Rejected non-editable transform field | Field: %s"), *TransformFieldName(field));
		RefreshFromSelectedPlaceable();
		FlashInvalidField(textBox);
		return false;
	}

	FTransform transform;
	UEditableTextBox* invalidTextBox = nullptr;
	if (!TryReadTransformFields(transform, invalidTextBox))
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Rejected transform field parse | Field: %s"), *TransformFieldName(field));
		RefreshFromSelectedPlaceable();
		FlashInvalidField(invalidTextBox ? invalidTextBox : textBox);
		return false;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioPlaceableContextMenuWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return false;
	}

	FString failureReason;
	if (!editorController->TryUpdateSelectedPlaceableTransform(transform, failureReason))
	{
		UE_LOG(
			LogScenarioPlaceableContextMenuWidget,
			Warning,
			TEXT("Rejected transform edit | Field: %s | Reason: %s"),
			*TransformFieldName(field),
			*failureReason);
		RefreshFromSelectedPlaceable();
		FlashInvalidField(textBox);
		return false;
	}

	RefreshFromSelectedPlaceable();
	return true;
}

bool UScenarioPlaceableContextMenuWidget::TryReadTransformFields(
	FTransform& outTransform,
	UEditableTextBox*& outInvalidTextBox) const
{
	outInvalidTextBox = nullptr;

	double locationX = 0.0;
	double locationY = 0.0;
	double locationZ = 0.0;
	double rotationX = 0.0;
	double rotationY = 0.0;
	double rotationZ = 0.0;
	double scaleX = 1.0;
	double scaleY = 1.0;
	double scaleZ = 1.0;

	struct FFieldRead
	{
		UEditableTextBox* TextBox = nullptr;
		double* Value = nullptr;
	};

	const FFieldRead reads[] =
	{
		{ LocationXTextBox.Get(), &locationX },
		{ LocationYTextBox.Get(), &locationY },
		{ LocationZTextBox.Get(), &locationZ },
		{ RotationXTextBox.Get(), &rotationX },
		{ RotationYTextBox.Get(), &rotationY },
		{ RotationZTextBox.Get(), &rotationZ },
		{ ScaleXTextBox.Get(), &scaleX },
		{ ScaleYTextBox.Get(), &scaleY },
		{ ScaleZTextBox.Get(), &scaleZ }
	};

	for (const FFieldRead& read : reads)
	{
		if (!TryReadDoubleField(read.TextBox, *read.Value))
		{
			outInvalidTextBox = read.TextBox;
			return false;
		}
	}

	if (scaleX <= 0.0 || scaleY <= 0.0 || scaleZ <= 0.0)
	{
		outInvalidTextBox = scaleX <= 0.0 ? ScaleXTextBox.Get() : scaleY <= 0.0 ? ScaleYTextBox.Get() : ScaleZTextBox.Get();
		return false;
	}

	const FVector location(locationX, locationY, locationZ);
	const FRotator rotation = FRotator::MakeFromEuler(FVector(rotationX, rotationY, rotationZ));
	const FVector scale(scaleX, scaleY, scaleZ);
	outTransform = FTransform(rotation, location, scale);
	return true;
}

bool UScenarioPlaceableContextMenuWidget::TryReadDoubleField(UEditableTextBox* textBox, double& outValue) const
{
	if (!textBox) return false;

	const FString stringValue = textBox->GetText().ToString().TrimStartAndEnd();
	if (!LexTryParseString(outValue, *stringValue) || !FMath::IsFinite(outValue))
	{
		return false;
	}

	return true;
}

bool UScenarioPlaceableContextMenuWidget::IsTransformFieldEditable(
	const UScenarioPlaceableComponent* placeableComponent,
	EScenarioPlaceableContextMenuTransformField field) const
{
	if (!placeableComponent) return false;

	switch (field)
	{
	case EScenarioPlaceableContextMenuTransformField::LocationX:
	case EScenarioPlaceableContextMenuTransformField::LocationY:
	case EScenarioPlaceableContextMenuTransformField::LocationZ:
		return placeableComponent->bAuthoringAllowLocationEdit;
	case EScenarioPlaceableContextMenuTransformField::RotationX:
	case EScenarioPlaceableContextMenuTransformField::RotationY:
	case EScenarioPlaceableContextMenuTransformField::RotationZ:
		return placeableComponent->bAuthoringAllowRotationEdit;
	case EScenarioPlaceableContextMenuTransformField::ScaleX:
	case EScenarioPlaceableContextMenuTransformField::ScaleY:
	case EScenarioPlaceableContextMenuTransformField::ScaleZ:
		return placeableComponent->bAuthoringAllowScaleEdit;
	default:
		return false;
	}
}

void UScenarioPlaceableContextMenuWidget::ApplyEditPermissions(
	const UScenarioPlaceableComponent* placeableComponent)
{
	const bool bCanRename = placeableComponent && placeableComponent->bAuthoringRenamable;
	const bool bCanDelete = placeableComponent && placeableComponent->bAuthoringDeletable;
	if (EditInstanceIdButton)
	{
		EditInstanceIdButton->SetVisibility(bCanRename ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		EditInstanceIdButton->SetIsEnabled(bCanRename);
	}
	if (InstanceIdEditableText)
	{
		InstanceIdEditableText->SetVisibility(bCanRename ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		InstanceIdEditableText->SetIsReadOnly(!bCanRename);
		InstanceIdEditableText->SetIsEnabled(bCanRename);
	}
	if (DeleteButton)
	{
		DeleteButton->SetVisibility(bCanDelete ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		DeleteButton->SetIsEnabled(bCanDelete);
	}

	const bool bCanEditLocation = placeableComponent && placeableComponent->bAuthoringAllowLocationEdit;
	const bool bCanEditRotation = placeableComponent && placeableComponent->bAuthoringAllowRotationEdit;
	const bool bCanEditScale = placeableComponent && placeableComponent->bAuthoringAllowScaleEdit;

	if (LocationSizeBox)
	{
		LocationSizeBox->SetVisibility(bCanEditLocation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RotationSizeBox)
	{
		RotationSizeBox->SetVisibility(bCanEditRotation ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (ScaleSizeBox)
	{
		ScaleSizeBox->SetVisibility(bCanEditScale ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	SetTextBoxEditable(LocationXTextBox, bCanEditLocation);
	SetTextBoxEditable(LocationYTextBox, bCanEditLocation);
	SetTextBoxEditable(LocationZTextBox, bCanEditLocation);
	SetTextBoxEditable(RotationXTextBox, bCanEditRotation);
	SetTextBoxEditable(RotationYTextBox, bCanEditRotation);
	SetTextBoxEditable(RotationZTextBox, bCanEditRotation);
	SetTextBoxEditable(ScaleXTextBox, bCanEditScale);
	SetTextBoxEditable(ScaleYTextBox, bCanEditScale);
	SetTextBoxEditable(ScaleZTextBox, bCanEditScale);
}

void UScenarioPlaceableContextMenuWidget::ApplyOrientationControls(
	const UScenarioPlaceableComponent* placeableComponent)
{
	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	const bool bCanEditOrientation = placeableComponent
		&& editorController
		&& editorController->CanEditTransformGizmoOrientationForSelection();
	const ESlateVisibility orientationVisibility = bCanEditOrientation
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed;

	if (OrientationSizeBox)
	{
		OrientationSizeBox->SetVisibility(orientationVisibility);
	}
	if (WorldOrientationButton)
	{
		WorldOrientationButton->SetVisibility(orientationVisibility);
	}
	if (RelativeOrientationButton)
	{
		RelativeOrientationButton->SetVisibility(orientationVisibility);
	}

	if (!bCanEditOrientation)
	{
		return;
	}

	const EScenarioTransformGizmoOrientationMode effectiveOrientationMode =
		editorController->GetEffectiveTransformGizmoOrientationMode();
	if (WorldOrientationButton)
	{
		WorldOrientationButton->SetIsEnabled(
			effectiveOrientationMode != EScenarioTransformGizmoOrientationMode::World);
	}
	if (RelativeOrientationButton)
	{
		RelativeOrientationButton->SetIsEnabled(
			effectiveOrientationMode != EScenarioTransformGizmoOrientationMode::Relative);
	}
}

void UScenarioPlaceableContextMenuWidget::SetTextBoxEditable(
	UEditableTextBox* textBox,
	bool bEditable) const
{
	if (!textBox) return;

	textBox->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	textBox->SetIsReadOnly(!bEditable);
	textBox->SetIsEnabled(bEditable);
}

void UScenarioPlaceableContextMenuWidget::FlashInvalidField(UEditableTextBox* textBox)
{
	if (!textBox) return;

	SetTextBoxFieldColor(textBox, InvalidFieldTextColor);

	UWorld* world = GetWorld();
	if (!world) return;

	TWeakObjectPtr weakTextBox(textBox);
	FTimerDelegate resetDelegate = FTimerDelegate::CreateWeakLambda(
		this,
		[this, weakTextBox]
		{
			if (UEditableTextBox* validTextBox = weakTextBox.Get())
			{
				SetTextBoxFieldColor(validTextBox, NormalFieldTextColor);
			}
		});

	FTimerHandle timerHandle;
	world->GetTimerManager().SetTimer(timerHandle, resetDelegate, InvalidFieldFlashSeconds, false);
}

void UScenarioPlaceableContextMenuWidget::SetTextBoxFieldColor(UEditableTextBox* textBox, const FLinearColor& color) const
{
	if (!textBox) return;

	const FSlateColor slateColor(color);
	FEditableTextBoxStyle textBoxStyle = textBox->GetWidgetStyle();
	textBoxStyle.SetForegroundColor(slateColor);
	textBoxStyle.SetReadOnlyForegroundColor(slateColor);
	textBoxStyle.SetFocusedForegroundColor(slateColor);
	textBox->SetWidgetStyle(textBoxStyle);
	textBox->SynchronizeProperties();
	textBox->InvalidateLayoutAndVolatility();
}

void UScenarioPlaceableContextMenuWidget::FlashInvalidInstanceIdField()
{
	if (!InstanceIdEditableText) return;

	SetInstanceIdFieldColor(InvalidFieldTextColor);

	UWorld* world = GetWorld();
	if (!world) return;

	TWeakObjectPtr<UEditableText> weakEditableText(InstanceIdEditableText);
	FTimerDelegate resetDelegate = FTimerDelegate::CreateWeakLambda(
		this,
		[this, weakEditableText]()
		{
			if (UEditableText* validEditableText = weakEditableText.Get())
			{
				FEditableTextStyle textStyle = validEditableText->WidgetStyle;
				textStyle.ColorAndOpacity = FSlateColor(NormalFieldTextColor);
				validEditableText->WidgetStyle = textStyle;
				validEditableText->SynchronizeProperties();
			}
		});

	FTimerHandle timerHandle;
	world->GetTimerManager().SetTimer(timerHandle, resetDelegate, InvalidFieldFlashSeconds, false);
}

void UScenarioPlaceableContextMenuWidget::SetInstanceIdFieldColor(const FLinearColor& color)
{
	if (!InstanceIdEditableText) return;

	FEditableTextStyle textStyle = InstanceIdEditableText->WidgetStyle;
	textStyle.ColorAndOpacity = FSlateColor(color);
	InstanceIdEditableText->WidgetStyle = textStyle;
	InstanceIdEditableText->SynchronizeProperties();
	InstanceIdEditableText->InvalidateLayoutAndVolatility();
}

void UScenarioPlaceableContextMenuWidget::SetInstanceIdFieldText(const FString& instanceId)
{
	if (InstanceIdEditableText)
	{
		InstanceIdEditableText->SetText(FText::FromString(instanceId));
		SetInstanceIdFieldColor(NormalFieldTextColor);
	}
}

void UScenarioPlaceableContextMenuWidget::SetTransformFieldTexts(const FTransform& transform)
{
	const FVector location = transform.GetLocation();
	const FVector rotationEuler = transform.GetRotation().Rotator().Euler();
	const FVector scale = transform.GetScale3D();

	SetFieldText(LocationXTextBox, location.X);
	SetFieldText(LocationYTextBox, location.Y);
	SetFieldText(LocationZTextBox, location.Z);
	SetFieldText(RotationXTextBox, rotationEuler.X);
	SetFieldText(RotationYTextBox, rotationEuler.Y);
	SetFieldText(RotationZTextBox, rotationEuler.Z);
	SetFieldText(ScaleXTextBox, scale.X);
	SetFieldText(ScaleYTextBox, scale.Y);
	SetFieldText(ScaleZTextBox, scale.Z);
}

void UScenarioPlaceableContextMenuWidget::SetFieldText(UEditableTextBox* textBox, double value) const
{
	if (textBox)
	{
		textBox->SetText(MakeNumberText(value));
		SetTextBoxFieldColor(textBox, NormalFieldTextColor);
	}
}
