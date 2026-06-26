#include "Scenario/Replay/ScenarioReplayDeveloperSettings.h"

// Groups replay camera settings under the existing OdiroSim project settings category.
FName UScenarioReplayDeveloperSettings::GetCategoryName() const
{
	return TEXT("OdiroSim");
}
