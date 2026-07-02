#include "Platform/Widget/OdiroCommonUserWidget.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"

namespace
{
	bool ShouldClearRuntimeTransactionFlags(const UUserWidget* widget)
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
		if (!userWidget || !ShouldClearRuntimeTransactionFlags(userWidget))
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

void UOdiroCommonUserWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ClearRuntimeTransactionFlagsForWidget(this);
}

void UOdiroCommonUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ClearRuntimeTransactionFlagsForWidget(this);
}

void UOdiroCommonUserWidget::ClearRuntimeTransactionFlagsForWidget(UUserWidget* widget)
{
	if (!ShouldClearRuntimeTransactionFlags(widget))
	{
		return;
	}

	TSet<UObject*> visitedObjects;
	ClearTransactionFlagsRecursive(widget, visitedObjects);
}
