#include "Platform/Widget/OdiroActivatableScreenWidget.h"

bool UOdiroActivatableScreenWidget::NativeOnHandleBackAction()
{
	if (!bHandleBackAction)
	{
		return Super::NativeOnHandleBackAction();
	}

	DeactivateWidget();
	return true;
}
