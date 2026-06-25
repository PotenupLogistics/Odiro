#include "Scenario/Widget/ScenarioEditorRouteMarkerOverlayWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Scenario/Editor/ScenarioAuthoringSubsystem.h"
#include "Scenario/ScenarioSimulationSubsystem.h"
#include "Scenario/Widget/ScenarioEditorRootWidget.h"
#include "Styling/CoreStyle.h"

namespace
{
	const TCHAR* DefaultRobotStartMarkerOverlayTexturePath =
		TEXT("/Game/Textures/Scenario/T_StartPointMarker.T_StartPointMarker");
	const TCHAR* DefaultRobotGoalMarkerOverlayTexturePath =
		TEXT("/Game/Textures/Scenario/T_GoalPointMarker.T_GoalPointMarker");
}

void UScenarioEditorRouteMarkerOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	LoadDefaultMarkerOverlayTextures();
}

int32 UScenarioEditorRouteMarkerOverlayWidget::NativePaint(
	const FPaintArgs& args,
	const FGeometry& allottedGeometry,
	const FSlateRect& myCullingRect,
	FSlateWindowElementList& outDrawElements,
	const int32 layerId,
	const FWidgetStyle& inWidgetStyle,
	const bool bParentEnabled) const
{
	const int32 markerLayerId = PaintRobotRouteMarkerOverlays(allottedGeometry, outDrawElements, layerId);
	return Super::NativePaint(
		args,
		allottedGeometry,
		myCullingRect,
		outDrawElements,
		markerLayerId,
		inWidgetStyle,
		bParentEnabled);
}

void UScenarioEditorRouteMarkerOverlayWidget::ApplyStyleFromRootWidget(
	const UScenarioEditorRootWidget* rootWidget)
{
	if (!rootWidget)
	{
		return;
	}

	RobotStartMarkerOverlayTexture = rootWidget->RobotStartMarkerOverlayTexture;
	RobotGoalMarkerOverlayTexture = rootWidget->RobotGoalMarkerOverlayTexture;
	RobotRouteMarkerOverlaySize = rootWidget->RobotRouteMarkerOverlaySize;
	RobotRouteMarkerOverlayAnchor = rootWidget->RobotRouteMarkerOverlayAnchor;
	RobotStartMarkerOverlayTint = rootWidget->RobotStartMarkerOverlayTint;
	RobotGoalMarkerOverlayTint = rootWidget->RobotGoalMarkerOverlayTint;
	LoadDefaultMarkerOverlayTextures();
}

void UScenarioEditorRouteMarkerOverlayWidget::LoadDefaultMarkerOverlayTextures()
{
	if (!RobotStartMarkerOverlayTexture)
	{
		RobotStartMarkerOverlayTexture =
			LoadObject<UTexture2D>(nullptr, DefaultRobotStartMarkerOverlayTexturePath);
	}
	if (!RobotGoalMarkerOverlayTexture)
	{
		RobotGoalMarkerOverlayTexture =
			LoadObject<UTexture2D>(nullptr, DefaultRobotGoalMarkerOverlayTexturePath);
	}
}

int32 UScenarioEditorRouteMarkerOverlayWidget::PaintRobotRouteMarkerOverlays(
	const FGeometry& allottedGeometry,
	FSlateWindowElementList& outDrawElements,
	const int32 layerId) const
{
	const UScenarioAuthoringSubsystem* authoringSubsystem =
		GetWorld() ? GetWorld()->GetSubsystem<UScenarioAuthoringSubsystem>() : nullptr;

	TArray<FScenarioEditorRouteMarkerOverlayItem> items;
	if (authoringSubsystem)
	{
		authoringSubsystem->GetRobotRouteMarkerOverlayItems(items);
	}
	if (items.IsEmpty())
	{
		const UScenarioSimulationSubsystem* simulationSubsystem =
			GetWorld() ? GetWorld()->GetSubsystem<UScenarioSimulationSubsystem>() : nullptr;
		if (simulationSubsystem)
		{
			simulationSubsystem->GetRobotRouteMarkerOverlayItems(items);
		}
	}

	int32 maxLayerId = layerId;
	for (const FScenarioEditorRouteMarkerOverlayItem& item : items)
	{
		FVector2D localPosition = FVector2D::ZeroVector;
		if (!TryProjectRouteMarkerOverlayPosition(item.WorldLocation, allottedGeometry, localPosition))
		{
			continue;
		}

		maxLayerId = FMath::Max(
			maxLayerId,
			PaintRobotRouteMarkerOverlayItem(item, localPosition, allottedGeometry, outDrawElements, layerId));
	}
	return maxLayerId;
}

int32 UScenarioEditorRouteMarkerOverlayWidget::PaintRobotRouteMarkerOverlayItem(
	const FScenarioEditorRouteMarkerOverlayItem& item,
	const FVector2D& localPosition,
	const FGeometry& allottedGeometry,
	FSlateWindowElementList& outDrawElements,
	const int32 layerId) const
{
	const FVector2D safeMarkerSize(
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.X),
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.Y));
	const FVector2D markerSize = safeMarkerSize;
	const FVector2D safeAnchor(
		FMath::Clamp(RobotRouteMarkerOverlayAnchor.X, 0.0, 1.0),
		FMath::Clamp(RobotRouteMarkerOverlayAnchor.Y, 0.0, 1.0));
	const FVector2D topLeft = localPosition - markerSize * safeAnchor;
	const FVector2D localSize = allottedGeometry.GetLocalSize();
	if (topLeft.X > localSize.X
		|| topLeft.Y > localSize.Y
		|| topLeft.X + markerSize.X < 0.0
		|| topLeft.Y + markerSize.Y < 0.0)
	{
		return layerId;
	}

	UTexture2D* markerTexture = item.Kind == EScenarioEditorRouteMarkerKind::Start
		? RobotStartMarkerOverlayTexture.Get()
		: RobotGoalMarkerOverlayTexture.Get();
	const FLinearColor baseTint = item.Kind == EScenarioEditorRouteMarkerKind::Start
		? RobotStartMarkerOverlayTint
		: RobotGoalMarkerOverlayTint;

	if (markerTexture)
	{
		FSlateBrush markerBrush;
		markerBrush.SetResourceObject(markerTexture);
		markerBrush.ImageSize = markerSize;
		markerBrush.DrawAs = ESlateBrushDrawType::Image;
		FSlateDrawElement::MakeBox(
			outDrawElements,
			layerId,
			allottedGeometry.ToPaintGeometry(markerSize, FSlateLayoutTransform(topLeft)),
			&markerBrush,
			ESlateDrawEffect::None,
			baseTint);
		return layerId + 1;
	}

	const FSlateBrush* whiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	if (whiteBrush)
	{
		const FVector2D headSize(markerSize.X * 0.74, markerSize.X * 0.74);
		const FVector2D headTopLeft(
			topLeft.X + (markerSize.X - headSize.X) * 0.5,
			topLeft.Y + markerSize.Y * 0.08);
		FSlateDrawElement::MakeBox(
			outDrawElements,
			layerId,
			allottedGeometry.ToPaintGeometry(headSize, FSlateLayoutTransform(headTopLeft)),
			whiteBrush,
			ESlateDrawEffect::None,
			baseTint);

		const FVector2D tip(topLeft.X + markerSize.X * 0.5, topLeft.Y + markerSize.Y);
		const FVector2D leftBase(topLeft.X + markerSize.X * 0.24, topLeft.Y + markerSize.Y * 0.42);
		const FVector2D rightBase(topLeft.X + markerSize.X * 0.76, topLeft.Y + markerSize.Y * 0.42);
		TArray<FVector2D> markerLines;
		markerLines.Reserve(4);
		markerLines.Add(leftBase);
		markerLines.Add(tip);
		markerLines.Add(rightBase);
		markerLines.Add(leftBase);
		FSlateDrawElement::MakeLines(
			outDrawElements,
			layerId + 1,
			allottedGeometry.ToPaintGeometry(),
			markerLines,
			ESlateDrawEffect::None,
			baseTint,
			true,
			3.0f);
		return layerId + 2;
	}

	return layerId;
}

bool UScenarioEditorRouteMarkerOverlayWidget::TryProjectRouteMarkerOverlayPosition(
	const FVector& worldLocation,
	const FGeometry& allottedGeometry,
	FVector2D& outLocalPosition) const
{
	APlayerController* owningPlayer = GetOwningPlayer();
	if (!owningPlayer)
	{
		return false;
	}

	FVector2D widgetPosition = FVector2D::ZeroVector;
	if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
		owningPlayer,
		worldLocation,
		widgetPosition,
		true))
	{
		return false;
	}

	const FGeometry viewportGeometry = UWidgetLayoutLibrary::GetViewportWidgetGeometry(this);
	const FVector2D overlayOriginInViewport = viewportGeometry.AbsoluteToLocal(allottedGeometry.GetAbsolutePosition());
	outLocalPosition = widgetPosition - overlayOriginInViewport;
	const FVector2D localSize = allottedGeometry.GetLocalSize();
	const FVector2D markerSize(
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.X),
		FMath::Max(1.0, RobotRouteMarkerOverlaySize.Y));
	return outLocalPosition.X >= -markerSize.X
		&& outLocalPosition.Y >= -markerSize.Y
		&& outLocalPosition.X <= localSize.X + markerSize.X
		&& outLocalPosition.Y <= localSize.Y + markerSize.Y;
}
