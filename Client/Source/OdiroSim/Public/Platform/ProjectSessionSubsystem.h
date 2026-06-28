#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ProjectSessionSubsystem.generated.h"

// GameInstance lifetime owner for the user project selected before entering the scenario editor.
UCLASS(BlueprintType)
class ODIROSIM_API UProjectSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Stores the active user project root as a normalized absolute path.
	UFUNCTION(BlueprintCallable, Category = "Platform|Project")
	void SetActiveProjectPath(const FString& projectPath);

	// Clears the selected user project for the current game instance.
	UFUNCTION(BlueprintCallable, Category = "Platform|Project")
	void ClearActiveProject();

	// Returns whether a user project root is available for editor/control UI.
	UFUNCTION(BlueprintPure, Category = "Platform|Project")
	bool HasActiveProject() const { return !ActiveProjectPath.IsEmpty(); }

	// Returns the normalized absolute user project root.
	UFUNCTION(BlueprintPure, Category = "Platform|Project")
	FString GetActiveProjectPath() const { return ActiveProjectPath; }

	// Returns <UserProject>/scenario.json for the active project.
	UFUNCTION(BlueprintPure, Category = "Platform|Project")
	FString GetActiveProjectScenarioPath() const;

	// Returns <UserProject>/setting.json for the active project.
	UFUNCTION(BlueprintPure, Category = "Platform|Project")
	FString GetActiveProjectSettingPath() const;

	// Returns <UserProject>/profile.json for the active project.
	UFUNCTION(BlueprintPure, Category = "Platform|Project")
	FString GetActiveProjectProfilePath() const;

private:
	static FString NormalizeProjectPath(const FString& projectPath);
	static FString BuildProjectFilePath(const FString& projectPath, const TCHAR* fileName);

	UPROPERTY(Transient)
	FString ActiveProjectPath;
};
