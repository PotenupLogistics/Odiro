#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ViewModel/ProjectCreateScreenViewModel.h"

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Platform/ProjectSessionSubsystem.h"
#include "Platform/SimulatorLaunchSubsystem.h"

namespace
{
	// ProjectCreate ViewModel이 사용하는 project-open config section.
	const TCHAR* ProjectCreateVmConfigSection = TEXT("OdiroSim.Platform.ProjectOpen");

	// Migration fallback을 검증하기 위해 테스트 중 함께 격리하는 legacy section.
	const TCHAR* ProjectCreateVmLegacyConfigSection = TEXT("OdiroSim.StartupMenu.ProjectOpen");

	// ProjectCreate ViewModel이 저장하는 create option key 목록.
	const TCHAR* ProjectCreateVmParentFolderKey = TEXT("ParentFolder");
	const TCHAR* ProjectCreateVmProjectNameKey = TEXT("ProjectName");
	const TCHAR* ProjectCreateVmScenarioPresetIdKey = TEXT("ScenarioPresetId");
	const TCHAR* ProjectCreateVmProfilePresetIdKey = TEXT("ProfilePresetId");
	const TCHAR* ProjectCreateVmPolicyPresetIdKey = TEXT("PolicyPresetId");
	const TCHAR* ProjectCreateVmRecentProjectPathsKey = TEXT("RecentProjectPaths");

	// 테스트 전용 project directory root를 만든다.
	FString MakeProjectCreateVmTestRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation/ProjectCreateScreenViewModel"),
			FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	// ProjectCreate preset card item 목록에 기대 preset id가 포함되는지 확인한다.
	bool HasProjectCreatePresetItem(const TArray<FProjectCreatePresetItem>& presetItems, const FString& presetId)
	{
		return presetItems.ContainsByPredicate(
			[&presetId](const FProjectCreatePresetItem& presetItem)
			{
				return presetItem.PresetId.Equals(presetId, ESearchCase::CaseSensitive);
			});
	}

	// 테스트 시작 전 저장된 create option snapshot.
	struct FProjectCreateVmConfigSnapshot
	{
		void Capture(const TCHAR* section)
		{
			bHadParentFolder = GConfig->GetString(section, ProjectCreateVmParentFolderKey, ParentFolder, GGameUserSettingsIni);
			bHadProjectName = GConfig->GetString(section, ProjectCreateVmProjectNameKey, ProjectName, GGameUserSettingsIni);
			bHadScenarioPresetId = GConfig->GetString(section, ProjectCreateVmScenarioPresetIdKey, ScenarioPresetId, GGameUserSettingsIni);
			bHadProfilePresetId = GConfig->GetString(section, ProjectCreateVmProfilePresetIdKey, ProfilePresetId, GGameUserSettingsIni);
			bHadPolicyPresetId = GConfig->GetString(section, ProjectCreateVmPolicyPresetIdKey, PolicyPresetId, GGameUserSettingsIni);
			GConfig->GetArray(section, ProjectCreateVmRecentProjectPathsKey, RecentProjectPaths, GGameUserSettingsIni);
		}

		void Clear(const TCHAR* section) const
		{
			GConfig->RemoveKey(section, ProjectCreateVmParentFolderKey, GGameUserSettingsIni);
			GConfig->RemoveKey(section, ProjectCreateVmProjectNameKey, GGameUserSettingsIni);
			GConfig->RemoveKey(section, ProjectCreateVmScenarioPresetIdKey, GGameUserSettingsIni);
			GConfig->RemoveKey(section, ProjectCreateVmProfilePresetIdKey, GGameUserSettingsIni);
			GConfig->RemoveKey(section, ProjectCreateVmPolicyPresetIdKey, GGameUserSettingsIni);
			GConfig->RemoveKey(section, ProjectCreateVmRecentProjectPathsKey, GGameUserSettingsIni);
		}

		void Restore(const TCHAR* section) const
		{
			RestoreString(section, ProjectCreateVmParentFolderKey, bHadParentFolder, ParentFolder);
			RestoreString(section, ProjectCreateVmProjectNameKey, bHadProjectName, ProjectName);
			RestoreString(section, ProjectCreateVmScenarioPresetIdKey, bHadScenarioPresetId, ScenarioPresetId);
			RestoreString(section, ProjectCreateVmProfilePresetIdKey, bHadProfilePresetId, ProfilePresetId);
			RestoreString(section, ProjectCreateVmPolicyPresetIdKey, bHadPolicyPresetId, PolicyPresetId);
			if (RecentProjectPaths.IsEmpty())
			{
				GConfig->RemoveKey(section, ProjectCreateVmRecentProjectPathsKey, GGameUserSettingsIni);
			}
			else
			{
				GConfig->SetArray(section, ProjectCreateVmRecentProjectPathsKey, RecentProjectPaths, GGameUserSettingsIni);
			}
		}

		// 단일 string config 값을 원래 상태로 복원한다.
		void RestoreString(const TCHAR* section, const TCHAR* key, const bool bHadValue, const FString& value) const
		{
			if (bHadValue)
			{
				GConfig->SetString(section, key, *value, GGameUserSettingsIni);
			}
			else
			{
				GConfig->RemoveKey(section, key, GGameUserSettingsIni);
			}
		}

		FString ParentFolder;
		FString ProjectName;
		FString ScenarioPresetId;
		FString ProfilePresetId;
		FString PolicyPresetId;
		TArray<FString> RecentProjectPaths;
		bool bHadParentFolder = false;
		bool bHadProjectName = false;
		bool bHadScenarioPresetId = false;
		bool bHadProfilePresetId = false;
		bool bHadPolicyPresetId = false;
	};

	// 테스트 중 변경한 create/recent config를 원래 사용자 값으로 복원한다.
	struct FScopedProjectCreateVmConfigRestore
	{
		FScopedProjectCreateVmConfigRestore()
		{
			if (!GConfig)
			{
				return;
			}

			Current.Capture(ProjectCreateVmConfigSection);
			Legacy.Capture(ProjectCreateVmLegacyConfigSection);
			Current.Clear(ProjectCreateVmConfigSection);
			Legacy.Clear(ProjectCreateVmLegacyConfigSection);
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		~FScopedProjectCreateVmConfigRestore()
		{
			if (!GConfig)
			{
				return;
			}

			Current.Restore(ProjectCreateVmConfigSection);
			Legacy.Restore(ProjectCreateVmLegacyConfigSection);
			GConfig->Flush(false, GGameUserSettingsIni);
		}

		FProjectCreateVmConfigSnapshot Current;
		FProjectCreateVmConfigSnapshot Legacy;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectCreateScreenViewModelCreateProjectTest,
	"OdiroSim.PlatformUi.ViewModel.ProjectCreateScreen.CreateProject",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectCreateScreenViewModelCreateProjectTest::RunTest(const FString& parameters)
{
	(void)parameters;

	const FScopedProjectCreateVmConfigRestore configRestore;
	UGameInstance* gameInstance = NewObject<UGameInstance>();
	USimulatorLaunchSubsystem* simulatorLaunchSubsystem = NewObject<USimulatorLaunchSubsystem>(gameInstance);
	UProjectSessionSubsystem* projectSessionSubsystem = NewObject<UProjectSessionSubsystem>(gameInstance);
	UProjectCreateScreenViewModel* viewModel = NewObject<UProjectCreateScreenViewModel>();
	TestNotNull(TEXT("game instance created"), gameInstance);
	TestNotNull(TEXT("simulator launch subsystem created"), simulatorLaunchSubsystem);
	TestNotNull(TEXT("project session subsystem created"), projectSessionSubsystem);
	TestNotNull(TEXT("project create viewmodel created"), viewModel);
	if (!gameInstance || !simulatorLaunchSubsystem || !projectSessionSubsystem || !viewModel)
	{
		return false;
	}

	viewModel->SetSubsystemOverrides(simulatorLaunchSubsystem, projectSessionSubsystem);
	viewModel->InitializeForGameInstance(gameInstance);
	TestTrue(TEXT("scenario preset items available"), viewModel->GetScenarioPresetItems().Num() > 0);
	TestTrue(TEXT("scenario preset items include blank"), HasProjectCreatePresetItem(viewModel->GetScenarioPresetItems(), TEXT("blank")));
	TestTrue(TEXT("scenario preset items include barricade"), HasProjectCreatePresetItem(viewModel->GetScenarioPresetItems(), TEXT("barricade")));
	TestTrue(TEXT("scenario preset items include curved"), HasProjectCreatePresetItem(viewModel->GetScenarioPresetItems(), TEXT("curved")));
	TestTrue(TEXT("scenario preset items include s-curve"), HasProjectCreatePresetItem(viewModel->GetScenarioPresetItems(), TEXT("s-curve")));
	TestTrue(TEXT("profile preset items available"), viewModel->GetProfilePresetItems().Num() > 0);
	TestTrue(TEXT("policy preset items available"), viewModel->GetPolicyPresetItems().Num() > 0);

	const FString testRoot = MakeProjectCreateVmTestRoot();
	const FString projectName = TEXT("ProjectCreateVmProject");
	const FString projectPath = FPaths::Combine(testRoot, projectName);
	IFileManager::Get().DeleteDirectory(*testRoot, false, true);

	viewModel->SetProjectParentFolder(testRoot);
	viewModel->SetProjectName(projectName);
	viewModel->SelectPreset(EProjectCreatePresetCategory::Scenario, TEXT("s-curve"));
	viewModel->SelectPreset(EProjectCreatePresetCategory::Profile, TEXT("full"));
	viewModel->SelectPreset(EProjectCreatePresetCategory::Policy, TEXT("demo"));
	TestTrue(TEXT("project can be created"), viewModel->CanCreateProject());
	TestEqual(TEXT("scenario summary updates"), viewModel->GetSelectedScenarioSummary(), FString(TEXT("s-curve.json")));
	TestEqual(TEXT("profile summary updates"), viewModel->GetSelectedProfileSummary(), FString(TEXT("full.json")));
	TestEqual(TEXT("policy summary updates"), viewModel->GetSelectedPolicySummary(), FString(TEXT("policy/demo")));

	TArray<FString> diagnostics;
	TestTrue(TEXT("create project through project create viewmodel"), viewModel->CreateProject(diagnostics));
	if (!diagnostics.IsEmpty())
	{
		AddInfo(FString::Printf(TEXT("project create diagnostics: %s"), *FString::Join(diagnostics, TEXT("\n"))));
	}
	TestTrue(TEXT("created project exists"), FPaths::DirectoryExists(projectPath));
	TestTrue(TEXT("project session active"), projectSessionSubsystem->HasActiveProject());
	TestTrue(TEXT("active project path set"), projectSessionSubsystem->GetActiveProjectPath().Equals(projectPath, ESearchCase::IgnoreCase));
	TestTrue(TEXT("created scenario exists"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("scenario.json"))));
	TestTrue(TEXT("created profile exists"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("profile.json"))));
	const FString settingPath = FPaths::Combine(projectPath, TEXT("setting.json"));
	TestTrue(TEXT("created setting exists"), FPaths::FileExists(settingPath));
	FString settingJson;
	TestTrue(TEXT("created setting loads"), FFileHelper::LoadFileToString(settingJson, *settingPath));
	TestTrue(
		TEXT("created setting project_id uses project name"),
		settingJson.Contains(FString::Printf(TEXT("\"project_id\": \"%s\""), *projectName)));
	TestTrue(TEXT("created policy exists"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("policy/action.py"))));
	TestFalse(TEXT("policy manifest not copied"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("policy/manifest.json"))));
	TestFalse(TEXT("policy thumbnail not copied"), FPaths::FileExists(FPaths::Combine(projectPath, TEXT("policy/thumbnail.png"))));

	TArray<FString> recentProjectPaths;
	if (GConfig)
	{
		GConfig->GetArray(
			ProjectCreateVmConfigSection,
			ProjectCreateVmRecentProjectPathsKey,
			recentProjectPaths,
			GGameUserSettingsIni);
	}
	TestTrue(TEXT("created project remembered"), recentProjectPaths.ContainsByPredicate(
		[&projectPath](const FString& recentProjectPath)
		{
			return recentProjectPath.Equals(projectPath, ESearchCase::IgnoreCase);
		}));

	IFileManager::Get().DeleteDirectory(*testRoot, false, true);
	return true;
}

#endif
