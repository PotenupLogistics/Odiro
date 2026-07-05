#include "Platform/Widget/PlatformWidgetRuntime.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"

namespace
{
	bool IsRuntimeWidgetContext(const UUserWidget* widget)
	{
		if (!widget || widget->IsDesignTime())
		{
			return false;
		}

		const UWorld* world = widget->GetWorld();
		if (!world)
		{
			return false;
		}

		return world->WorldType == EWorldType::PIE
			|| world->WorldType == EWorldType::Game
			|| world->WorldType == EWorldType::GamePreview
			|| world->WorldType == EWorldType::EditorPreview;
	}

	void ClearTransactionFlagsRecursive(UObject* object, TSet<UObject*>& visitedObjects)
	{
		if (!object || visitedObjects.Contains(object))
		{
			return;
		}

		visitedObjects.Add(object);
		object->ClearFlags(RF_Transactional);

		if (UWidget* widget = Cast<UWidget>(object))
		{
			ClearTransactionFlagsRecursive(widget->Slot, visitedObjects);
		}

		UUserWidget* userWidget = Cast<UUserWidget>(object);
		if (!userWidget || !IsRuntimeWidgetContext(userWidget))
		{
			return;
		}

		UWidgetTree* widgetTree = userWidget->WidgetTree;
		if (!widgetTree)
		{
			return;
		}

		ClearTransactionFlagsRecursive(widgetTree, visitedObjects);
		widgetTree->ForEachWidget([&visitedObjects](UWidget* childWidget)
		{
			ClearTransactionFlagsRecursive(childWidget, visitedObjects);
		});
	}
}

void PlatformWidgetRuntime::ApplyFullscreenViewportSlot(UUserWidget* Widget)
{
	if (!IsRuntimeWidgetContext(Widget))
	{
		return;
	}

	const FVector2D viewportSize = UWidgetLayoutLibrary::GetViewportSize(Widget);
	if (viewportSize.X <= 0.0f || viewportSize.Y <= 0.0f)
	{
		return;
	}

	Widget->SetAlignmentInViewport(FVector2D::ZeroVector);
	Widget->SetPositionInViewport(FVector2D::ZeroVector, false);
	Widget->SetDesiredSizeInViewport(viewportSize);
}

void PlatformWidgetRuntime::ClearRuntimeTransactionFlags(UUserWidget* Widget)
{
	if (!IsRuntimeWidgetContext(Widget))
	{
		return;
	}

	TSet<UObject*> visitedObjects;
	ClearTransactionFlagsRecursive(Widget, visitedObjects);
}
