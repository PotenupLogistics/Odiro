#include "Episode/Widget/EpisodePlaceableContextMenuWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Episode/Components/EpisodePlaceableComponent.h"
#include "Episode/Editor/EpisodeEditorController.h"
#include "GameFramework/Actor.h"
#include "Styling/SlateTypes.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogEpisodePlaceableContextMenuWidget, Log, All);

namespace
{
	FText MakeNumberText(double value)
	{
		return FText::FromString(FString::Printf(TEXT("%.3f"), value));
	}

	FString TransformFieldName(EEpisodePlaceableContextMenuTransformField field)
	{
		switch (field)
		{
		case EEpisodePlaceableContextMenuTransformField::LocationX:
			return TEXT("LocationX");
		case EEpisodePlaceableContextMenuTransformField::LocationY:
			return TEXT("LocationY");
		case EEpisodePlaceableContextMenuTransformField::LocationZ:
			return TEXT("LocationZ");
		case EEpisodePlaceableContextMenuTransformField::RotationX:
			return TEXT("RotationX");
		case EEpisodePlaceableContextMenuTransformField::RotationY:
			return TEXT("RotationY");
		case EEpisodePlaceableContextMenuTransformField::RotationZ:
			return TEXT("RotationZ");
		case EEpisodePlaceableContextMenuTransformField::ScaleX:
			return TEXT("ScaleX");
		case EEpisodePlaceableContextMenuTransformField::ScaleY:
			return TEXT("ScaleY");
		case EEpisodePlaceableContextMenuTransformField::ScaleZ:
			return TEXT("ScaleZ");
		default:
			return TEXT("Unknown");
		}
	}
}

void UEpisodePlaceableContextMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (EditInstanceIdButton)
	{
		EditInstanceIdButton->OnClicked.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleEditInstanceIdButtonClicked);
		EditInstanceIdButton->OnClicked.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleEditInstanceIdButtonClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleDeleteButtonClicked);
		DeleteButton->OnClicked.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleDeleteButtonClicked);
	}
	if (InstanceIdEditableText)
	{
		InstanceIdEditableText->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleInstanceIdCommitted);
		InstanceIdEditableText->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleInstanceIdCommitted);
	}

	if (LocationXTextBox)
	{
		LocationXTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleLocationXCommitted);
		LocationXTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleLocationXCommitted);
	}
	if (LocationYTextBox)
	{
		LocationYTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleLocationYCommitted);
		LocationYTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleLocationYCommitted);
	}
	if (LocationZTextBox)
	{
		LocationZTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleLocationZCommitted);
		LocationZTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleLocationZCommitted);
	}

	if (RotationXTextBox)
	{
		RotationXTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleRotationXCommitted);
		RotationXTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleRotationXCommitted);
	}
	if (RotationYTextBox)
	{
		RotationYTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleRotationYCommitted);
		RotationYTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleRotationYCommitted);
	}
	if (RotationZTextBox)
	{
		RotationZTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleRotationZCommitted);
		RotationZTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleRotationZCommitted);
	}

	if (ScaleXTextBox)
	{
		ScaleXTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleScaleXCommitted);
		ScaleXTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleScaleXCommitted);
	}
	if (ScaleYTextBox)
	{
		ScaleYTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleScaleYCommitted);
		ScaleYTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleScaleYCommitted);
	}
	if (ScaleZTextBox)
	{
		ScaleZTextBox->OnTextCommitted.RemoveDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleScaleZCommitted);
		ScaleZTextBox->OnTextCommitted.AddDynamic(this, &UEpisodePlaceableContextMenuWidget::HandleScaleZCommitted);
	}
}

void UEpisodePlaceableContextMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RequestEditorWidgetInputMode();
	RefreshFromSelectedPlaceable();
}

void UEpisodePlaceableContextMenuWidget::NativeDestruct()
{
	ReleaseEditorWidgetInputMode();
	Super::NativeDestruct();
}

void UEpisodePlaceableContextMenuWidget::SetSelectedPlaceable(UEpisodePlaceableComponent* placeableComponent)
{
	SelectedPlaceableComponent = placeableComponent;
	RefreshFromSelectedPlaceable();
}

void UEpisodePlaceableContextMenuWidget::RefreshFromSelectedPlaceable()
{
	bRefreshingFields = true;

	UEpisodePlaceableComponent* placeableComponent = SelectedPlaceableComponent.Get();
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
	bRefreshingFields = false;
}

void UEpisodePlaceableContextMenuWidget::HandleEditInstanceIdButtonClicked()
{
	if (!InstanceIdEditableText)
	{
		return;
	}

	InstanceIdEditableText->SetKeyboardFocus();
}

void UEpisodePlaceableContextMenuWidget::HandleDeleteButtonClicked()
{
	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogEpisodePlaceableContextMenuWidget, Warning, TEXT("Owning player is not an EpisodeEditorController."));
		return;
	}

	FString failureReason;
	if (!editorController->DeleteSelectedPlaceable(failureReason))
	{
		UE_LOG(LogEpisodePlaceableContextMenuWidget, Warning, TEXT("Failed to delete selected placeable | %s"), *failureReason);
		FlashInvalidInstanceIdField();
	}
}

void UEpisodePlaceableContextMenuWidget::HandleInstanceIdCommitted(const FText& text, ETextCommit::Type commitMethod)
{
	if (bRefreshingFields || commitMethod == ETextCommit::OnCleared)
	{
		RefreshFromSelectedPlaceable();
		return;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogEpisodePlaceableContextMenuWidget, Warning, TEXT("Owning player is not an EpisodeEditorController."));
		return;
	}

	FString failureReason;
	if (!editorController->TryRenameSelectedPlaceableInstanceId(text.ToString().TrimStartAndEnd(), failureReason))
	{
		UE_LOG(LogEpisodePlaceableContextMenuWidget, Warning, TEXT("Rejected InstanceId edit | %s"), *failureReason);
		RefreshFromSelectedPlaceable();
		FlashInvalidInstanceIdField();
		return;
	}

	RefreshFromSelectedPlaceable();
}

void UEpisodePlaceableContextMenuWidget::HandleLocationXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationXTextBox, EEpisodePlaceableContextMenuTransformField::LocationX, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleLocationYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationYTextBox, EEpisodePlaceableContextMenuTransformField::LocationY, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleLocationZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(LocationZTextBox, EEpisodePlaceableContextMenuTransformField::LocationZ, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleRotationXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationXTextBox, EEpisodePlaceableContextMenuTransformField::RotationX, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleRotationYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationYTextBox, EEpisodePlaceableContextMenuTransformField::RotationY, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleRotationZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(RotationZTextBox, EEpisodePlaceableContextMenuTransformField::RotationZ, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleScaleXCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleXTextBox, EEpisodePlaceableContextMenuTransformField::ScaleX, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleScaleYCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleYTextBox, EEpisodePlaceableContextMenuTransformField::ScaleY, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::HandleScaleZCommitted(const FText&, ETextCommit::Type commitMethod)
{
	CommitTransformField(ScaleZTextBox, EEpisodePlaceableContextMenuTransformField::ScaleZ, commitMethod);
}

void UEpisodePlaceableContextMenuWidget::RequestEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		editorController->RequestEditorWidgetInputMode(this);
	}
}

void UEpisodePlaceableContextMenuWidget::ReleaseEditorWidgetInputMode()
{
	if (AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer()))
	{
		editorController->ReleaseEditorWidgetInputMode(this);
	}
}

bool UEpisodePlaceableContextMenuWidget::CommitTransformField(
	UEditableTextBox* textBox,
	EEpisodePlaceableContextMenuTransformField field,
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

	FTransform transform;
	UEditableTextBox* invalidTextBox = nullptr;
	if (!TryReadTransformFields(transform, invalidTextBox))
	{
		UE_LOG(LogEpisodePlaceableContextMenuWidget, Warning, TEXT("Rejected transform field parse | Field: %s"), *TransformFieldName(field));
		RefreshFromSelectedPlaceable();
		FlashInvalidField(invalidTextBox ? invalidTextBox : textBox);
		return false;
	}

	AEpisodeEditorController* editorController = Cast<AEpisodeEditorController>(GetOwningPlayer());
	if (!editorController)
	{
		UE_LOG(LogEpisodePlaceableContextMenuWidget, Warning, TEXT("Owning player is not an EpisodeEditorController."));
		return false;
	}

	FString failureReason;
	if (!editorController->TryUpdateSelectedPlaceableTransform(transform, failureReason))
	{
		UE_LOG(
			LogEpisodePlaceableContextMenuWidget,
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

bool UEpisodePlaceableContextMenuWidget::TryReadTransformFields(
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

bool UEpisodePlaceableContextMenuWidget::TryReadDoubleField(UEditableTextBox* textBox, double& outValue) const
{
	if (!textBox) return false;

	const FString stringValue = textBox->GetText().ToString().TrimStartAndEnd();
	if (!LexTryParseString(outValue, *stringValue) || !FMath::IsFinite(outValue))
	{
		return false;
	}

	return true;
}

void UEpisodePlaceableContextMenuWidget::FlashInvalidField(UEditableTextBox* textBox)
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

void UEpisodePlaceableContextMenuWidget::SetTextBoxFieldColor(UEditableTextBox* textBox, const FLinearColor& color) const
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

void UEpisodePlaceableContextMenuWidget::FlashInvalidInstanceIdField()
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

void UEpisodePlaceableContextMenuWidget::SetInstanceIdFieldColor(const FLinearColor& color)
{
	if (!InstanceIdEditableText) return;

	FEditableTextStyle textStyle = InstanceIdEditableText->WidgetStyle;
	textStyle.ColorAndOpacity = FSlateColor(color);
	InstanceIdEditableText->WidgetStyle = textStyle;
	InstanceIdEditableText->SynchronizeProperties();
	InstanceIdEditableText->InvalidateLayoutAndVolatility();
}

void UEpisodePlaceableContextMenuWidget::SetInstanceIdFieldText(const FString& instanceId)
{
	if (InstanceIdEditableText)
	{
		InstanceIdEditableText->SetText(FText::FromString(instanceId));
		SetInstanceIdFieldColor(NormalFieldTextColor);
	}
}

void UEpisodePlaceableContextMenuWidget::SetTransformFieldTexts(const FTransform& transform)
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

void UEpisodePlaceableContextMenuWidget::SetFieldText(UEditableTextBox* textBox, double value) const
{
	if (textBox)
	{
		textBox->SetText(MakeNumberText(value));
		SetTextBoxFieldColor(textBox, NormalFieldTextColor);
	}
}
