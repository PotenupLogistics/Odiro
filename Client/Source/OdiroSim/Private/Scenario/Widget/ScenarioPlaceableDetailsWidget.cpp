#include "Scenario/Widget/ScenarioPlaceableDetailsWidget.h"

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

DEFINE_LOG_CATEGORY_STATIC(LogScenarioPlaceableDetailsWidget, Log, All);

namespace
{
	FText MakeNumberText(double value)
	{
		return FText::FromString(FString::Printf(TEXT("%.3f"), value));
	}

	FString TransformFieldName(EScenarioPlaceableDetailsTransformField field)
	{
		switch (field)
		{
		case EScenarioPlaceableDetailsTransformField::LocationX:
			return TEXT("LocationX");
		case EScenarioPlaceableDetailsTransformField::LocationY:
			return TEXT("LocationY");
		case EScenarioPlaceableDetailsTransformField::LocationZ:
			return TEXT("LocationZ");
		case EScenarioPlaceableDetailsTransformField::RotationX:
			return TEXT("RotationX");
		case EScenarioPlaceableDetailsTransformField::RotationY:
			return TEXT("RotationY");
		case EScenarioPlaceableDetailsTransformField::RotationZ:
			return TEXT("RotationZ");
		case EScenarioPlaceableDetailsTransformField::ScaleX:
			return TEXT("ScaleX");
		case EScenarioPlaceableDetailsTransformField::ScaleY:
			return TEXT("ScaleY");
		case EScenarioPlaceableDetailsTransformField::ScaleZ:
			return TEXT("ScaleZ");
		default:
			return TEXT("Unknown");
		}
	}
}

void UScenarioPlaceableDetailsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EditInstanceIdButton)
	{
		EditInstanceIdButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleEditInstanceIdButtonClicked);
		EditInstanceIdButton->OnClicked.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleEditInstanceIdButtonClicked);
	}
	if (WorldOrientationButton)
	{
		WorldOrientationButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleWorldOrientationButtonClicked);
		WorldOrientationButton->OnClicked.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleWorldOrientationButtonClicked);
	}
	if (RelativeOrientationButton)
	{
		RelativeOrientationButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRelativeOrientationButtonClicked);
		RelativeOrientationButton->OnClicked.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRelativeOrientationButtonClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleDeleteButtonClicked);
		DeleteButton->OnClicked.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleDeleteButtonClicked);
	}
	if (InstanceIdEditableText)
	{
		InstanceIdEditableText->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleInstanceIdCommitted);
		InstanceIdEditableText->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleInstanceIdCommitted);
	}

	if (LocationXTextBox)
	{
		LocationXTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleLocationXCommitted);
		LocationXTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleLocationXCommitted);
	}
	if (LocationYTextBox)
	{
		LocationYTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleLocationYCommitted);
		LocationYTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleLocationYCommitted);
	}
	if (LocationZTextBox)
	{
		LocationZTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleLocationZCommitted);
		LocationZTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleLocationZCommitted);
	}

	if (RotationXTextBox)
	{
		RotationXTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRotationXCommitted);
		RotationXTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRotationXCommitted);
	}
	if (RotationYTextBox)
	{
		RotationYTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRotationYCommitted);
		RotationYTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRotationYCommitted);
	}
	if (RotationZTextBox)
	{
		RotationZTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRotationZCommitted);
		RotationZTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleRotationZCommitted);
	}

	if (ScaleXTextBox)
	{
		ScaleXTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleScaleXCommitted);
		ScaleXTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleScaleXCommitted);
	}
	if (ScaleYTextBox)
	{
		ScaleYTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleScaleYCommitted);
		ScaleYTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleScaleYCommitted);
	}
	if (ScaleZTextBox)
	{
		ScaleZTextBox->OnTextCommitted.RemoveDynamic(this, &UScenarioPlaceableDetailsWidget::HandleScaleZCommitted);
		ScaleZTextBox->OnTextCommitted.AddDynamic(this, &UScenarioPlaceableDetailsWidget::HandleScaleZCommitted);
	}
}

void UScenarioPlaceableDetailsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RequestEditorWidgetInputMode();
	RefreshFromSelectedPlaceable();
}

void UScenarioPlaceableDetailsWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

void UScenarioPlaceableDetailsWidget::SetSelectedPlaceable(UScenarioPlaceableComponent* placeableComponent)
{
	SelectedPlaceableComponent = placeableComponent;
	RefreshFromSelectedPlaceable();
}

void UScenarioPlaceableDetailsWidget::RefreshFromSelectedPlaceable()
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

void UScenarioPlaceableDetailsWidget::HandleEditInstanceIdButtonClicked()
{
	if (!InstanceIdEditableText)
	{
		return;
	}

	InstanceIdEditableText->SetKeyboardFocus();
}

void UScenarioPlaceableDetailsWidget::HandleWorldOrientationButtonClicked()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode::World);
		RefreshFromSelectedPlaceable();
	}
}

void UScenarioPlaceableDetailsWidget::HandleRelativeOrientationButtonClicked()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->SetTransformGizmoOrientationMode(EScenarioTransformGizmoOrientationMode::Relative);
		RefreshFromSelectedPlaceable();
	}
}

void UScenarioPlaceableDetailsWidget::HandleDeleteButtonClicked()
{
	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return;
	}

	FString failureReason;
	UScenarioPlaceableComponent* placeableComponent = SelectedPlaceableComponent.Get();
	if (!placeableComponent || !placeableComponent->bAuthoringDeletable)
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Selected placeable cannot be deleted."));
		FlashInvalidInstanceIdField();
		return;
	}

	if (!editorController->DeleteSelectedPlaceable(failureReason))
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Failed to delete selected placeable | %s"), *failureReason);
		FlashInvalidInstanceIdField();
	}
}

void UScenarioPlaceableDetailsWidget::HandleInstanceIdCommitted(const FText& text, ETextCommit::Type commitMethod)
{
	if (bRefreshingFields || commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromSelectedPlaceable();
		return;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return;
	}

	UScenarioPlaceableComponent* placeableComponent = SelectedPlaceableComponent.Get();
	if (!placeableComponent || !placeableComponent->bAuthoringRenamable)
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Selected placeable cannot be renamed."));
		RefreshFromSelectedPlaceable();
		FlashInvalidInstanceIdField();
		return;
	}

	FString failureReason;
	if (!editorController->TryRenameSelectedPlaceableInstanceId(text.ToString().TrimStartAndEnd(), failureReason))
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Rejected InstanceId edit | %s"), *failureReason);
		RefreshFromSelectedPlaceable();
		FlashInvalidInstanceIdField();
		return;
	}

	RefreshFromSelectedPlaceable();
}

void UScenarioPlaceableDetailsWidget::HandleLocationXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationXTextBox, EScenarioPlaceableDetailsTransformField::LocationX, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleLocationYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationYTextBox, EScenarioPlaceableDetailsTransformField::LocationY, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleLocationZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationZTextBox, EScenarioPlaceableDetailsTransformField::LocationZ, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleRotationXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationXTextBox, EScenarioPlaceableDetailsTransformField::RotationX, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleRotationYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationYTextBox, EScenarioPlaceableDetailsTransformField::RotationY, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleRotationZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationZTextBox, EScenarioPlaceableDetailsTransformField::RotationZ, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleScaleXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleXTextBox, EScenarioPlaceableDetailsTransformField::ScaleX, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleScaleYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleYTextBox, EScenarioPlaceableDetailsTransformField::ScaleY, commitMethod);
}

void UScenarioPlaceableDetailsWidget::HandleScaleZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleZTextBox, EScenarioPlaceableDetailsTransformField::ScaleZ, commitMethod);
}

void UScenarioPlaceableDetailsWidget::RequestEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->RequestEditorWidgetInputMode(this);
	}
}

void UScenarioPlaceableDetailsWidget::ReleaseEditorWidgetInputMode()
{
	if (AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer()))
	{
		editorController->ReleaseEditorWidgetInputMode(this);
	}
}

bool UScenarioPlaceableDetailsWidget::CommitTransformField(
	UEditableTextBox* textBox,
	EScenarioPlaceableDetailsTransformField field,
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
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Rejected non-editable transform field | Field: %s"), *TransformFieldName(field));
		RefreshFromSelectedPlaceable();
		FlashInvalidField(textBox);
		return false;
	}

	FTransform transform;
	UEditableTextBox* invalidTextBox = nullptr;
	if (!TryReadTransformFields(transform, invalidTextBox))
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Rejected transform field parse | Field: %s"), *TransformFieldName(field));
		RefreshFromSelectedPlaceable();
		FlashInvalidField(invalidTextBox ? invalidTextBox : textBox);
		return false;
	}

	AScenarioEditorController* editorController = Cast<AScenarioEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogScenarioPlaceableDetailsWidget, Warning, TEXT("Owning player is not an ScenarioEditorController."));
		return false;
	}

	FString failureReason;
	if (!editorController->TryUpdateSelectedPlaceableTransform(transform, failureReason))
	{
		UE_LOG(
			LogScenarioPlaceableDetailsWidget,
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

bool UScenarioPlaceableDetailsWidget::TryReadTransformFields(
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

bool UScenarioPlaceableDetailsWidget::TryReadDoubleField(UEditableTextBox* textBox, double& outValue) const
{
	if (!textBox) return false;

	const FString stringValue = textBox->GetText().ToString().TrimStartAndEnd();
	if (!LexTryParseString(outValue, *stringValue) || !FMath::IsFinite(outValue))
	{
		return false;
	}

	return true;
}

bool UScenarioPlaceableDetailsWidget::IsTransformFieldEditable(
	const UScenarioPlaceableComponent* placeableComponent,
	EScenarioPlaceableDetailsTransformField field) const
{
	if (!placeableComponent) return false;

	switch (field)
	{
	case EScenarioPlaceableDetailsTransformField::LocationX:
	case EScenarioPlaceableDetailsTransformField::LocationY:
	case EScenarioPlaceableDetailsTransformField::LocationZ:
		return placeableComponent->bAuthoringAllowLocationEdit;
	case EScenarioPlaceableDetailsTransformField::RotationX:
	case EScenarioPlaceableDetailsTransformField::RotationY:
	case EScenarioPlaceableDetailsTransformField::RotationZ:
		return placeableComponent->bAuthoringAllowRotationEdit;
	case EScenarioPlaceableDetailsTransformField::ScaleX:
	case EScenarioPlaceableDetailsTransformField::ScaleY:
	case EScenarioPlaceableDetailsTransformField::ScaleZ:
		return placeableComponent->bAuthoringAllowScaleEdit;
	default:
		return false;
	}
}

void UScenarioPlaceableDetailsWidget::ApplyEditPermissions(
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

void UScenarioPlaceableDetailsWidget::ApplyOrientationControls(
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

void UScenarioPlaceableDetailsWidget::SetTextBoxEditable(
	UEditableTextBox* textBox,
	bool bEditable) const
{
	if (!textBox) return;

	textBox->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	textBox->SetIsReadOnly(!bEditable);
	textBox->SetIsEnabled(bEditable);
}

void UScenarioPlaceableDetailsWidget::FlashInvalidField(UEditableTextBox* textBox)
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

void UScenarioPlaceableDetailsWidget::SetTextBoxFieldColor(UEditableTextBox* textBox, const FLinearColor& color) const
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

void UScenarioPlaceableDetailsWidget::FlashInvalidInstanceIdField()
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

void UScenarioPlaceableDetailsWidget::SetInstanceIdFieldColor(const FLinearColor& color)
{
	if (!InstanceIdEditableText) return;

	FEditableTextStyle textStyle = InstanceIdEditableText->WidgetStyle;
	textStyle.ColorAndOpacity = FSlateColor(color);
	InstanceIdEditableText->WidgetStyle = textStyle;
	InstanceIdEditableText->SynchronizeProperties();
	InstanceIdEditableText->InvalidateLayoutAndVolatility();
}

void UScenarioPlaceableDetailsWidget::SetInstanceIdFieldText(const FString& instanceId)
{
	if (InstanceIdEditableText)
	{
		InstanceIdEditableText->SetText(FText::FromString(instanceId));
		SetInstanceIdFieldColor(NormalFieldTextColor);
	}
}

void UScenarioPlaceableDetailsWidget::SetTransformFieldTexts(const FTransform& transform)
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

void UScenarioPlaceableDetailsWidget::SetFieldText(UEditableTextBox* textBox, double value) const
{
	if (textBox)
	{
		textBox->SetText(MakeNumberText(value));
		SetTextBoxFieldColor(textBox, NormalFieldTextColor);
	}
}
