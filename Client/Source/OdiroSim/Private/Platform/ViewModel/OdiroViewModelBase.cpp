#include "Platform/ViewModel/OdiroViewModelBase.h"

void UOdiroViewModelBase::SetDiagnosticsText(const FString& message)
{
	UE_MVVM_SET_PROPERTY_VALUE(DiagnosticsText, message);
}

void UOdiroViewModelBase::ClearDiagnostics()
{
	SetDiagnosticsText(FString());
}

void UOdiroViewModelBase::SetBusy(const bool bInBusy)
{
	UE_MVVM_SET_PROPERTY_VALUE(bBusy, bInBusy);
}
