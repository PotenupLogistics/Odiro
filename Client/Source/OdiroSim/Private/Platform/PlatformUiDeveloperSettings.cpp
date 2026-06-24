#include "Platform/PlatformUiDeveloperSettings.h"

// Groups Platform UI settings under the existing OdiroSim project settings category.
FName UPlatformUiDeveloperSettings::GetCategoryName() const
{
	return TEXT("OdiroSim");
}
